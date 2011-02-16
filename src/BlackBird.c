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
#include <getopt.h>
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

//-----------------------------------------------------------------------------
// Typedefs:
//-----------------------------------------------------------------------------

typedef struct _CLIENT

{
    int clifd;    // Client socket file descriptor.
}

CLIENT, *PCLIENT;

typedef struct _CORE

{
    pthread_t t_acc;    // Accept worker thread ID.
    pthread_t t_wai;    // Wait worker thread ID.
    int epfd;           // Epoll handler.
}

CORE, *PCORE;

typedef struct _SERVER

{
    int srvfd;     // Server socket file descriptor.
    int cores;     // Number of system cores.
    PCORE core;    // Will point to an array of cores.
}

SERVER, *PSERVER;

//-----------------------------------------------------------------------------
// Prototypes:
//-----------------------------------------------------------------------------

void *W_Acce(void *arg);    // Acce Worker.
void *W_Wait(void *arg);    // Wait Worker.
void *W_Data(void *arg);    // Data Worker.

//-----------------------------------------------------------------------------
// Globals:
//-----------------------------------------------------------------------------

SERVER s;    // Main server structure.

//-----------------------------------------------------------------------------
// Entry point:
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])

{
    // Initializations:
    int i, c, do_help;                    // For general use.
    pthread_t thread = pthread_self();    // Main thread ID (myself).
    cpu_set_t cpuset;                     // Each bit represents a CPU.
    pthread_attr_t attr;                  // Pthread attribute variable.
    struct sockaddr_in srvaddr;           // IPv4 socket address structure.

    // Parse command line options:
    struct option longopts[] = {
    { "max-clients",  optional_argument,  NULL,        'c' },
    { "max-active",   optional_argument,  NULL,        'a' },
    { "help",         no_argument,        & do_help,     1 },
    { 0, 0, 0, 0 }};

    while((c = getopt_long(argc, argv, "c::a::", longopts, NULL)) != -1)

    {
        switch(c)

        {
            case 'c': break;
            case 'a': break;
        }
    }

    // Initialize server and core structures:
    s.cores = sysconf(_SC_NPROCESSORS_ONLN); s.srvfd = -1;
    if((s.core = malloc(sizeof(CORE) * s.cores)) == NULL) MyDBG(end0);
    for(i=0; i<s.cores; i++){s.core[i].epfd = -1;}

    // Server blocking socket. Go ahead and reuse it:
    if((s.srvfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) MyDBG(end1);
    i=1; if(setsockopt(s.srvfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0) MyDBG(end2);

    // Initialize srvaddr:
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srvaddr.sin_port = htons(LISTENP);

    // Bind and listen:
    if(bind(s.srvfd, (struct sockaddr *) &srvaddr, sizeof(srvaddr)) < 0) MyDBG(end2);
    if(listen(s.srvfd, LISTENQ) < 0) MyDBG(end2);

    // Threads are explicitly created in a joinable state:
    if(pthread_attr_init(&attr) != 0) MyDBG(end2);
    if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0) MyDBG(end3);

    // For each core in the system:
    for(i=0; i<s.cores; i++)

    {
        // Open an epoll fd dimensioned for DESCRIPTORS_HINT descriptors:
        if((s.core[i].epfd = epoll_create(DESCRIPTORS_HINT)) < 0) MyDBG(end4);

        // New threads inherits a copy of its creator's CPU affinity mask:
        CPU_ZERO(&cpuset); CPU_SET(i, &cpuset);
        if(pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end4);

        // Create the Acce Worker and the Wait Worker:
        if(pthread_create(&s.core[i].t_acc, &attr, W_Acce, (void *) &s.core[i]) != 0) MyDBG(end4);
        if(pthread_create(&s.core[i].t_wai, &attr, W_Wait, (void *) &s.core[i]) != 0) MyDBG(end4);
    }

    // Restore creator's (myself) affinity to all available cores:
    CPU_ZERO(&cpuset); for(i=0; i<s.cores; i++){CPU_SET(i, &cpuset);}
    if(pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end4);

    // Pre-threading a pool of Data Workers:

    // Free library resources used by the attribute:
    pthread_attr_destroy(&attr);

    // Explicitly join all worker threads:
    for(i=0; i<s.cores; i++)

    {
        if(pthread_join(s.core[i].t_acc, NULL) != 0) MyDBG(end4);
        if(pthread_join(s.core[i].t_wai, NULL) != 0) MyDBG(end4);
    }

    // Return on succes:
    pthread_exit(NULL);

    // Return on error:
    end4: for(i=0; i<s.cores; i++){close(s.core[i].epfd);}
    end3: pthread_attr_destroy(&attr);
    end2: close(s.srvfd);
    end1: free(s.core);
    end0: return -1;
}

//-----------------------------------------------------------------------------
// W_Acce:
//-----------------------------------------------------------------------------

void *W_Acce(void *arg)

{
    // Initializations:
    int i, clifd;                       // Socket file descriptor.
    struct sockaddr_in cliaddr;         // IPv4 socket address structure.
    socklen_t len = sizeof(cliaddr);    // Fixed length (16 bytes).
    PCORE core = (PCORE)arg;            // Pointer to core data.
    PCLIENT cptr = NULL;                // Pointer to client data.
    struct epoll_event ev;              // Epoll event structure.

    // Setup epoll behavior as one-shot-edge-triggered:
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    // Main thread loop:
    while(1)

    {
        // Blocking accept returns a non-blocking client socket:
        if((clifd = accept(s.srvfd, (struct sockaddr *) &cliaddr, &len)) < 0) MyDBG(end0);
        if((i = fcntl(clifd, F_GETFL)) < 0) MyDBG(end1);
        i |= O_NONBLOCK; if(fcntl(clifd, F_SETFL, i) < 0) MyDBG(end1);

        // Initialize the client data structure:
        if((cptr = malloc(sizeof(CLIENT))) == NULL) MyDBG(end1);
        cptr->clifd = clifd;

        // Return data to us later:
        ev.data.ptr = (void *)cptr;

        // Epoll assignment:
        if(epoll_ctl(core->epfd, EPOLL_CTL_ADD, clifd, &ev) < 0) MyDBG(end2);
        continue;

        // Client error:
        end2: free(cptr);
        end1: close(clifd);
    }

    // Return on error:
    end0: pthread_exit(NULL);
}

//-----------------------------------------------------------------------------
// W_Wait:
//-----------------------------------------------------------------------------

void *W_Wait(void *arg)

{
    // Initializations:
    int i, n;
    PCORE core = (PCORE)arg;             // Pointer to core data.
    PCLIENT cptr = NULL;                 // Pointer to client data.
    struct epoll_event ev [MAX_EVENTS];  // Epoll-events array.

    // Main thread loop:
    while(1)

    {
        // Wait up to MAX_EVENTS on the epoll-set:
        if((n = epoll_wait(core->epfd, &ev[0], MAX_EVENTS, -1)) < 0) MyDBG(end0);

        // For each event fired: if the fd is available to be read from
        // without blocking, it is transfered to the Data Workers pool:
        for(i=0; i<n; i++)

        {
            // Get the pointer to the client data:
            cptr = (PCLIENT)(ev[i].data.ptr);

            if(ev[i].events & EPOLLIN)

            {
                printf("[%d]\n", cptr->clifd);
            }
        }
    }

    // Return on error:
    end0: pthread_exit(NULL);
}

//-----------------------------------------------------------------------------
// W_Data:
//-----------------------------------------------------------------------------

void *W_Data(void *arg)

{
    // Return on error:
    pthread_exit(NULL);
}
