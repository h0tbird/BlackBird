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

//-----------------------------------------------------------------------------
// Typedefs:
//-----------------------------------------------------------------------------

typedef struct _CORE_STRC

{
    int epfd;       // Epoll handler.
    pthread_t tid;  // Thread ID.
}

CORE_STRC, *PCORE_STRC;

//-----------------------------------------------------------------------------
// Prototypes:
//-----------------------------------------------------------------------------

void *Worker(void *arg);

//-----------------------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------------------

#define MyDBG(x) do {printf("Error: %s:%d\n", __FILE__, __LINE__); goto x;} while (0)

#define DESCRIPTORS_HINT 100    // Just a hint to the kernel.
#define LISTENP 8080            // WebSockets port must be 80.
#define LISTENQ 1024            // sysctl -w net.core.somaxconn=1024

//-----------------------------------------------------------------------------
// Entry point:
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])

{
    // Initializations:
    int i, c = sysconf(_SC_NPROCESSORS_ONLN);   // Number of cores.
    pthread_t thread = pthread_self();          // Main thread ID (myself).
    cpu_set_t cpuset;                           // Each bit represents a CPU.
    CORE_STRC core[c];                          // Variable-length array (C99).
    for(i=0; i<c; i++){core[i].epfd = -1;}      // Invalid file descriptors.
    int listenfd, clientfd;                     // Socket file descriptors.
    struct sockaddr_in servaddr, cliaddr;       // IPv4 socket address structure.
    socklen_t len = sizeof(cliaddr);            // Fixed length (16 bytes).
    struct epoll_event ev;                      // Describes epoll behavior as
    ev.events = EPOLLIN | EPOLLET;              // non-blocking edge-triggered.

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

    // Blocking listen socket:
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) MyDBG(end0);

    // Initialize servaddr:
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(LISTENP);

    // Bind and listen:
    if(bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) MyDBG(end1);
    if(listen(listenfd, LISTENQ) < 0) MyDBG(end1);

    // Main loop:
    while(1)

    {
        // Non-blocking client socket:
        if((clientfd = accept(listenfd, (struct sockaddr *) &cliaddr, &len)) < 0) continue;
        if((i = fcntl(clientfd, F_GETFL)) < 0) MyDBG(end2);
        i |= O_NONBLOCK; if(fcntl(clientfd, F_SETFL, i) < 0) MyDBG(end2);

        // Complete the event structure:
        ev.data.fd = clientfd;
        continue;

        // Client error:
        end2: close(clientfd);
        continue;
    }

    // Return on success (never):
    pthread_exit(NULL);

    // Return on error:
    end1: close(listenfd);
    end0: for(i=0; i<c; i++){close(core[i].epfd);} return -1;
}

//-----------------------------------------------------------------------------
// Worker:
//-----------------------------------------------------------------------------

void *Worker(void *arg)

{
    // Initializations:
    CORE_STRC *core = (PCORE_STRC) arg;

    // Do something:
    sleep(2);

    // Return on success:
    pthread_exit(NULL);
}
