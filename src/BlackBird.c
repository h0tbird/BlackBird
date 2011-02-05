//-----------------------------------------------------------------------------
// gcc -Wall -pthread BlackBird.c
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes:
//-----------------------------------------------------------------------------

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

//-----------------------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------------------

#define MyDBG(x) do {printf("Error: %s:%d\n", __FILE__, __LINE__); goto x;} while (0)

#define DESCRIPTORS_HINT 100    // Just a hint to the kernel.
#define LISTENP 8080            // WebSockets port must be 80.
#define LISTENQ 1024            // sysctl -w net.core.somaxconn=1024
#define MAX_EVENTS 50           // Epoll max events.
#define MTU_SIZE 1500           // Read up to MTU_SIZE bytes.

//-----------------------------------------------------------------------------
// Typedefs:
//-----------------------------------------------------------------------------

typedef struct _CLIENT

{
    int clifd;              // Socket file descriptor.
    char buf[MTU_SIZE];     // Data buffer.
    ssize_t len;            // Valid data lenght.
}

CLIENT, *PCLIENT;

typedef struct _CORE

{
    pthread_t tid;  // Thread ID.
    int epfd;       // Epoll handler.
}

CORE, *PCORE;

//-----------------------------------------------------------------------------
// Prototypes:
//-----------------------------------------------------------------------------

void *Worker(void *arg);

//-----------------------------------------------------------------------------
// Entry point:
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])

{
    // Initializations:
    int i, j=0;                                 // For general use.
    int c = sysconf(_SC_NPROCESSORS_ONLN);      // Number of cores.
    pthread_t thread = pthread_self();          // Main thread ID (myself).
    cpu_set_t cpuset;                           // Each bit represents a CPU.
    CORE core[c];                               // Variable-length array (C99).
    for(i=0; i<c; i++){core[i].epfd = -1;}      // Invalid file descriptors.
    int srvfd, clifd;                           // Socket file descriptors.
    struct sockaddr_in srvaddr, cliaddr;        // IPv4 socket address structure.
    socklen_t len = sizeof(cliaddr);            // Fixed length (16 bytes).
    struct epoll_event ev;                      // Describes epoll behavior as
    ev.events = EPOLLIN | EPOLLET;              // non-blocking edge-triggered.
    PCLIENT cptr = NULL;                        // Pointer to client data.

    // For each core in the system:
    for(i=0; i<c; i++)

    {
        // Open an epoll fd dimensioned for DESCRIPTORS_HINT descriptors:
        if((core[i].epfd = epoll_create(DESCRIPTORS_HINT)) < 0) MyDBG(end0);

        // New threads inherits a copy of its creator's CPU affinity mask:
        CPU_ZERO(&cpuset); CPU_SET(i, &cpuset);
        if(pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end0);
        if(pthread_create(&core[i].tid, NULL, Worker, (void *) &core[i]) != 0) MyDBG(end0);
    }

    // Restore creator's affinity to all available cores:
    CPU_ZERO(&cpuset); for(i=0; i<c; i++){CPU_SET(i, &cpuset);}
    if(pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end0);

    // Blocking socket, go ahead and reuse it:
    if((srvfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) MyDBG(end0);
    i=1; if(setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0) MyDBG(end1);

    // Initialize srvaddr:
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srvaddr.sin_port = htons(LISTENP);

    // Bind and listen:
    if(bind(srvfd, (struct sockaddr *) &srvaddr, sizeof(srvaddr)) < 0) MyDBG(end1);
    if(listen(srvfd, LISTENQ) < 0) MyDBG(end1);

    // Main dispatcher loop:
    while(1)

    {
        // Non-blocking client socket:
        if((clifd = accept(srvfd, (struct sockaddr *) &cliaddr, &len)) < 0) continue;
        if((i = fcntl(clifd, F_GETFL)) < 0) MyDBG(end2);
        i |= O_NONBLOCK; if(fcntl(clifd, F_SETFL, i) < 0) MyDBG(end2);

        // Initialize the client data structure:
        if((cptr = malloc(sizeof(CLIENT))) == NULL) MyDBG(end2);
        cptr->clifd = clifd;

        // Return to us later, in the worker thread:
        ev.data.ptr = (void *)cptr;

        // Round-robin epoll assignment:
        if(j>c-1){j=0;}
        if(epoll_ctl(core[j].epfd, EPOLL_CTL_ADD, clifd, &ev) < 0) MyDBG(end3);
        j++; continue;

        // Client error:
        end3: free(cptr);
        end2: close(clifd);
        continue;
    }

    // Return on error:
    end1: close(srvfd);
    end0: for(i=0; i<c; i++){close(core[i].epfd);}
    return -1;
}

//-----------------------------------------------------------------------------
// Worker:
//-----------------------------------------------------------------------------

void *Worker(void *arg)

{
    // Initializations:
    int i, num;
    struct epoll_event ev [MAX_EVENTS];
    PCORE core = (PCORE)arg;
    PCLIENT cptr;
    ssize_t len;

    // Main worker loop:
    while(1)

    {
        // Wait up to MAX_EVENTS events on the epoll-set:
        if((num = epoll_wait(core->epfd, &ev[0], MAX_EVENTS, -1)) < 0) MyDBG(end0);

        // For each event fired:
        for(i=0; i<num; i++)

        {
            // The associated file kernel buffer is available for read:
            if(ev[i].events & EPOLLIN)

            {
                // Get the socket fd and try to read some data (non-blocking):
                (cptr = (PCLIENT)(ev[i].data.ptr))->len = 0;
                read: len = read(cptr->clifd, &(cptr->buf[cptr->len]), MTU_SIZE);

                // Data available:
                if(len > 0)

                {
                    printf("T:%u R:%d F:%d D:%d\n", (unsigned int)pthread_self(), num, cptr->clifd, (int)len);

                    // Read again until it would block:
                    cptr->len += len;
                    goto read;
                }

                // EAGAIN = EWOULDBLOCK:
                else if(len < 0 && errno == EAGAIN) continue;

                // End of connection:
                else if(len <= 0)

                {
                    if(epoll_ctl(core->epfd, EPOLL_CTL_DEL, cptr->clifd, NULL) < 0) MyDBG(end0);
                    close(cptr->clifd);
                    free(cptr);
                }
            }
        }
    }

    // Return on error:
    end0: pthread_exit(NULL);
}
