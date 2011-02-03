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

//-----------------------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------------------

#define MyDBG(x) do {printf("Error: %s:%d\n", __FILE__, __LINE__); goto x;} while (0)

#define DESCRIPTORS_HINT 100    // Just a hint to the kernel.
#define LISTENP 8080            // WebSockets port must be 80.
#define LISTENQ 1024            // sysctl -w net.core.somaxconn=1024
#define MAX_EVENTS 50           // Epoll max events.

//-----------------------------------------------------------------------------
// Typedefs:
//-----------------------------------------------------------------------------

typedef struct _CLIENT

{
    int clientfd;   // Socket file descriptor.
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
    int listenfd, clientfd;                     // Socket file descriptors.
    struct sockaddr_in servaddr, cliaddr;       // IPv4 socket address structure.
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
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) MyDBG(end0);
    i=1; if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0) MyDBG(end1);

    // Initialize servaddr:
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(LISTENP);

    // Bind and listen:
    if(bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) MyDBG(end1);
    if(listen(listenfd, LISTENQ) < 0) MyDBG(end1);

    // Main dispatcher loop:
    while(1)

    {
        // Non-blocking client socket:
        if((clientfd = accept(listenfd, (struct sockaddr *) &cliaddr, &len)) < 0) continue;
        if((i = fcntl(clientfd, F_GETFL)) < 0) MyDBG(end2);
        i |= O_NONBLOCK; if(fcntl(clientfd, F_SETFL, i) < 0) MyDBG(end2);

        // Initialize the client data structure:
        if((cptr = malloc(sizeof(CLIENT))) == NULL) MyDBG(end2);
        cptr->clientfd = clientfd;

        // Return to us later, in the worker thread:
        ev.data.ptr = (void *)cptr;

        // Round-robin epoll assignment:
        if(j>c-1){j=0;}
        if(epoll_ctl(core[j].epfd, EPOLL_CTL_ADD, clientfd, &ev) < 0) MyDBG(end3);
        j++; continue;

        // Client error:
        end3: free(cptr);
        end2: close(clientfd);
        continue;
    }

    // Return on error:
    end1: close(listenfd);
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

    // Main worker loop:
    while(1)

    {
        // Wait for events on the epoll set:
        if((num = epoll_wait(core->epfd, &ev[0], MAX_EVENTS, -1)) < 0) MyDBG(end0);

        // For each fd ready:
        for(i=0; i<num; i++)

        {
            // Data is ready in the kernel buffer:
            if(ev[i].events & EPOLLIN)
            {printf("Thread: %u Ready: %d Descriptor: %d\n", (unsigned int)pthread_self(), num, ((PCLIENT)(ev[i].data.ptr))->clientfd);}

            // Not interested:
            else {printf("Other event!\n");}
        }
    }

    // Return on error:
    end0: pthread_exit(NULL);
}
