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

#define MyDBG(x) do {printf("(%d) %s:%d\n", errno, __FILE__, __LINE__); goto x;} while (0)

#define MAX_CLIENTS 500         // Defaults for maxc.
#define MAX_ACTIVE 100          // Defaults for maxa.
#define MAX_EVENTS 50           // Defaults for maxe.
#define LISTENP 8080            // WebSockets port must be 80.
#define LISTENQ 1024            // sysctl -w net.core.somaxconn=1024

//-----------------------------------------------------------------------------
// Typedefs:
//-----------------------------------------------------------------------------

typedef struct _CLIENT

{
    int clifd;    // Client socket file descriptor.
    int epfd;     // Epoll monitoring clifd.
}

CLIENT, *PCLIENT;

typedef struct _CONF

{
    int maxc;    // Epoll size hint.
    int maxa;    // Pre-threading hint.
    int maxe;    // Epoll events per round.
}

CONF, *PCONF;

typedef struct _CORE

{
    pthread_t t_acc;    // Accept worker thread ID.
    pthread_t t_wai;    // Wait worker thread ID.
    int epfd;           // Epoll handler.
}

CORE, *PCORE;

typedef struct _SERVER

{
    int srvfd;         // Server socket file descriptor.
    int cores;         // Number of system cores.
    PCORE core;        // Will point to an array of cores.
    CONF cnf;          // Will store configuration options.
    PCLIENT *cli;      // Will store an array of client pointers.
    int iput, iget;    // Indexes for the cli array.
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

SERVER s;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cnd = PTHREAD_COND_INITIALIZER;

//-----------------------------------------------------------------------------
// Entry point:
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])

{
    // Initializations:
    int i;                                // For general use.
    pthread_t thread = pthread_self();    // Main thread ID (myself).
    cpu_set_t cpuset;                     // Each bit represents a CPU.
    pthread_attr_t attr;                  // Pthread attribute variable.
    struct sockaddr_in srvaddr;           // IPv4 socket address structure.

    // Set config defaults:
    s.cnf.maxc = MAX_CLIENTS;
    s.cnf.maxa = MAX_ACTIVE;
    s.cnf.maxe = MAX_EVENTS;

    // Parse command line options:
    struct option longopts[] = {
    { "max-clients",  required_argument,  NULL,  'c' },
    { "max-active",   required_argument,  NULL,  'a' },
    { "max-events",   required_argument,  NULL,  'e' },
    { 0, 0, 0, 0 }};

    while((i = getopt_long(argc, argv, "c:a:e:", longopts, NULL)) != -1)

    {
        if (i == -1) break;

        switch(i)

        {
            case 'c': s.cnf.maxc = atoi(optarg);
                      break;
            case 'a': s.cnf.maxa = atoi(optarg);
                      break;
            case 'e': s.cnf.maxe = atoi(optarg);
                      break;
            default:  abort();
        }
    }

    // Initialize server structures:
    s.cores = sysconf(_SC_NPROCESSORS_ONLN);
    s.srvfd = -1; s.iput = s.iget = 0;

    // Malloc an array of cores:
    if((s.core = malloc(sizeof(CORE) * s.cores)) == NULL) MyDBG(end0);
    for(i=0; i<s.cores; i++){s.core[i].epfd = -1;}

    // Malloc an array of client pointers:
    if((s.cli = malloc(sizeof(PCLIENT) * s.cnf.maxc)) == NULL) MyDBG(end1);
    for(i=0; i<s.cnf.maxc; i++){s.cli[i] = NULL;}

    // Server blocking socket. Go ahead and reuse it:
    if((s.srvfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) MyDBG(end2);
    i=1; if(setsockopt(s.srvfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0) MyDBG(end3);

    // Initialize srvaddr:
    bzero(&srvaddr, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srvaddr.sin_port = htons(LISTENP);

    // Bind and listen:
    if(bind(s.srvfd, (struct sockaddr *) &srvaddr, sizeof(srvaddr)) < 0) MyDBG(end3);
    if(listen(s.srvfd, LISTENQ) < 0) MyDBG(end3);

    // Threads are explicitly created in a joinable state:
    if(pthread_attr_init(&attr) != 0) MyDBG(end3);
    if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0) MyDBG(end4);

    // For each core in the system:
    for(i=0; i<s.cores; i++)

    {
        // Open an epoll fd dimensioned for s.cnf.maxc/s.cores descriptors:
        if((s.core[i].epfd = epoll_create(s.cnf.maxc/s.cores)) < 0) MyDBG(end5);

        // New threads inherits a copy of its creator's CPU affinity mask:
        CPU_ZERO(&cpuset); CPU_SET(i, &cpuset);
        if(pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end5);

        // Create the Acce Worker and the Wait Worker:
        if(pthread_create(&s.core[i].t_acc, &attr, W_Acce, (void *) &s.core[i]) != 0) MyDBG(end5);
        if(pthread_create(&s.core[i].t_wai, &attr, W_Wait, (void *) &s.core[i]) != 0) MyDBG(end5);
    }

    // Restore creator's (myself) affinity to all available cores:
    CPU_ZERO(&cpuset); for(i=0; i<s.cores; i++){CPU_SET(i, &cpuset);}
    if(pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end5);

    // Pre-threading a pool of s.cnf.maxa Data Workers:
    for(i=0; i<s.cnf.maxa; i++){if(pthread_create(&thread, &attr, W_Data, NULL) != 0) MyDBG(end5);}

    // Free library resources used by the attribute:
    pthread_attr_destroy(&attr);

    // Explicitly join worker threads:
    for(i=0; i<s.cores; i++)

    {
        if(pthread_join(s.core[i].t_acc, NULL) != 0) MyDBG(end5);
        if(pthread_join(s.core[i].t_wai, NULL) != 0) MyDBG(end5);
    }

    // Return on succes:
    pthread_exit(NULL);

    // Return on error:
    end5: for(i=0; i<s.cores; i++){close(s.core[i].epfd);}
    end4: pthread_attr_destroy(&attr);
    end3: close(s.srvfd);
    end2: free(s.cli);
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
        cptr->epfd = core->epfd;
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
    struct epoll_event ev[s.cnf.maxe];   // Epoll-events array (C99).

    // Main thread loop:
    while(1)

    {
        // Wait up to s.cnf.maxe on the epoll-set:
        if((n = epoll_wait(core->epfd, &ev[0], s.cnf.maxe, -1)) < 0) MyDBG(end0);

        // For each event fired: if the fd is available to be read from
        // without blocking, it is transfered to the Data Workers pool:
        for(i=0; i<n; i++)

        {
            if(ev[i].events & EPOLLIN)

            {
                // Enter the critical section:
                if(pthread_mutex_lock(&mtx) != 0) MyDBG(end0);

                // Get the pointer to the client data:
                s.cli[s.iput] = (PCLIENT)(ev[i].data.ptr);
                printf("[%d]\n", s.cli[s.iput]->clifd);
                if(++s.iput == s.cnf.maxc){s.iput=0;}
                if(s.iput == s.iget) MyDBG(end0);

                // Signal to awake sleeping thread:
                if(pthread_cond_signal(&cnd) != 0) MyDBG(end0);

                // Leave the critical section:
                if(pthread_mutex_unlock(&mtx) != 0) MyDBG(end0);
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
    // Initializations:
    PCLIENT cptr = NULL;

    // Main thread loop:
    while(1)

    {
        // Critical section:
        if(pthread_mutex_lock(&mtx) != 0) MyDBG(end0);
        while(s.iget == s.iput){if(pthread_cond_wait(&cnd, &mtx) != 0) MyDBG(end0);}
        cptr = s.cli[s.iget]; if(++s.iget == s.cnf.maxc){s.iget=0;}
        if(pthread_mutex_unlock(&mtx) != 0) MyDBG(end0);

        // Drain the socket:

        // Trigger re-arm:
    }

    // Return on error:
    end0: pthread_exit(NULL);
}
