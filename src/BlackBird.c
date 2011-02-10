/******************************************************************************
* Copyright (C) 2011 Marc Villacorta Morera
*
* Authors: Marc Villacorta Morera <marc.villacorta@gmail.com>
*
* This file is part of BlackBird.
*
* BlackBird is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* BlackBird is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with BlackBird. If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

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

#define DESCRIPTORS_HINT 250    // Just a hint to the kernel.
#define LISTENP 8080            // WebSockets port must be 80.
#define LISTENQ 1024            // sysctl -w net.core.somaxconn=1024
#define MAX_EVENTS 50           // Epoll max events per round.
#define MTU 1400                // Read up to MTU bytes per socket and round.
#define MAX_BUF MTU*10          // Store up to 10 MTUs.

//-----------------------------------------------------------------------------
// Typedefs:
//-----------------------------------------------------------------------------

typedef struct _CLIENT

{
    int clifd;              // Socket file descriptor.
    char ibuf[MAX_BUF];     // Shared Input data buffer.
    char obuf[MAX_BUF];     // Shared Output data buffer.
    ssize_t ilen, olen;     // Valid data lenght.
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

void *IO_Worker(void *arg);
int min(int x, int y);

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
    ev.events = EPOLLIN;                        // non-blocking level-triggered.
    PCLIENT cptr = NULL;                        // Pointer to client data.

    // For each core in the system:
    for(i=0; i<c; i++)

    {
        // Open an epoll fd dimensioned for DESCRIPTORS_HINT descriptors:
        if((core[i].epfd = epoll_create(DESCRIPTORS_HINT)) < 0) MyDBG(end0);

        // New threads inherits a copy of its creator's CPU affinity mask:
        CPU_ZERO(&cpuset); CPU_SET(i, &cpuset);
        if(pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end0);
        if(pthread_create(&core[i].tid, NULL, IO_Worker, (void *) &core[i]) != 0) MyDBG(end0);
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
        cptr->clifd = clifd; cptr->ilen = 0; cptr->olen = 0;

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
// IO_Worker:
//-----------------------------------------------------------------------------

void *IO_Worker(void *arg)

{
    // Initializations:
    int i, num;
    struct epoll_event ev [MAX_EVENTS];
    PCORE core = (PCORE)arg;
    PCLIENT cptr;
    ssize_t n;
    size_t len;

    // Main worker loop:
    while(1)

    {
        // Wait up to MAX_EVENTS events on the epoll-set:
        if((num = epoll_wait(core->epfd, &ev[0], MAX_EVENTS, -1)) < 0) MyDBG(end0);

        // For each event fired:
        for(i=0; i<num; i++)

        {
            // Get the pointer to the client data:
            cptr = (PCLIENT)(ev[i].data.ptr);

            // The file is available to be written to without blocking:
            if(ev[i].events & EPOLLOUT) {printf("EPOLLOUT\n");}

            // The file is available to be read from without blocking:
            if(ev[i].events & EPOLLIN)

            {
                // Initialize:
                len = 0;

                // Try to non-blocking read some data until it would block or MTU or MAX_BUF:
                read: n = read(cptr->clifd, &(cptr->ibuf[cptr->ilen]), min(MTU-len, MAX_BUF-(cptr->ilen)));
                if(n>0){len+=n; cptr->ilen+=n; printf("[%d]:%d:%d\n", cptr->clifd, cptr->ilen, (int)n); goto read;}

                // Ok, it would block or enough data readed for this round so jump to next fd:
                else if((n<0 && errno==EAGAIN) || (n==0 && (len==MTU || cptr->ilen==MAX_BUF))) continue;

                // The call was interrupted by a signal before any data was read:
                else if(n<0 && errno==EINTR) goto read;

                // End of connection:
                else if(n<=0)

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

//-----------------------------------------------------------------------------
// min: Function to find minimum of x and y
//-----------------------------------------------------------------------------

int min(int x, int y) {return y ^ ((x ^ y) & -(x < y));}
