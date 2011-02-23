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
#define MAX_THREADS 50          // Defaults for maxt.
#define MAX_EVENTS 50           // Defaults for maxe.
#define LISTENP 8080            // Server listen port.
#define LISTENQ 1024            // sysctl -w net.core.somaxconn=1024
#define MTU 2896                // 2*(1500-40-12) per socket and round.

//-----------------------------------------------------------------------------
// Typedefs:
//-----------------------------------------------------------------------------

typedef struct _CLIENT

{
    int clifd;    // Client socket file descriptor.
    int epfd;     // Epoll monitoring this clifd.
}

CLIENT, *PCLIENT;

typedef struct _CONF

{
    int maxc;    // Epoll size hint.
    int maxt;    // Pre-threading hint.
    int maxe;    // Epoll events per round.
}

CONF, *PCONF;

typedef struct _SERVER

{
    int srvfd;         // Server socket file descriptor.
    int cores;         // Number of system cores.
    int iput, iget;    // Indexes for the cli array.
    int *epfd;         // Will point to an epfd array.
    CONF cnf;          // Will store configuration options.
    PCLIENT *cli;      // Will point to a pointer array.
}

SERVER, *PSERVER;

//-----------------------------------------------------------------------------
// Prototypes:
//-----------------------------------------------------------------------------

void *W_Acce(void *arg);    // Acce Worker.
void *W_Wait(void *arg);    // Wait Worker.
void *W_Data(void *arg);    // Data Worker.

int parser(char *buff, int len, PCLIENT cptr);

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
    int i;                         // For general use.
    pthread_t thread;              // Main thread ID (myself).
    cpu_set_t cpuset;              // Each bit represents a CPU.
    struct sockaddr_in srvaddr;    // IPv4 socket address structure.

    // Set config defaults:
    s.cnf.maxc = MAX_CLIENTS;
    s.cnf.maxt = MAX_THREADS;
    s.cnf.maxe = MAX_EVENTS;

    // Parse command line options:
    struct option longopts[] = {
    { "max-clients",  required_argument,  NULL,  'c' },
    { "max-threads",  required_argument,  NULL,  't' },
    { "max-events",   required_argument,  NULL,  'e' },
    { 0, 0, 0, 0 }};

    while((i = getopt_long(argc, argv, "c:a:e:", longopts, NULL)) != -1)

    {
        if (i == -1) break;

        switch(i)

        {
            case 'c': s.cnf.maxc = atoi(optarg);
                      break;
            case 't': s.cnf.maxt = atoi(optarg);
                      break;
            case 'e': s.cnf.maxe = atoi(optarg);
                      break;
            default:  abort();
        }
    }

    // Initialize server structures:
    s.cores = sysconf(_SC_NPROCESSORS_ONLN);
    s.iput = s.iget = 0;

    // Malloc epfd and client pointers:
    if((s.epfd = malloc(sizeof(int) * s.cores)) == NULL) MyDBG(end0);
    if((s.cli = malloc(sizeof(PCLIENT) * s.cnf.maxc)) == NULL) MyDBG(end1);

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

    // For each core in the system:
    for(i=0; i<s.cores; i++)

    {
        // Open an epoll fd dimensioned for s.cnf.maxc/s.cores descriptors:
        if((s.epfd[i] = epoll_create(s.cnf.maxc/s.cores)) < 0) MyDBG(end3);

        // New threads inherits a copy of its creator's CPU affinity mask:
        CPU_ZERO(&cpuset); CPU_SET(i, &cpuset);
        if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end3);

        // Create the Acce Worker and the Wait Worker:
        if(pthread_create(&thread, NULL, W_Acce, (void *) s.epfd[i]) != 0) MyDBG(end3);
        if(pthread_create(&thread, NULL, W_Wait, (void *) s.epfd[i]) != 0) MyDBG(end3);
    }

    // Restore creator's (myself) affinity to all available cores:
    CPU_ZERO(&cpuset); for(i=0; i<s.cores; i++){CPU_SET(i, &cpuset);}
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end3);

    // Pre-threading a pool of s.cnf.maxt Data Workers:
    for(i=0; i<s.cnf.maxt; i++){if(pthread_create(&thread, NULL, W_Data, NULL) != 0) MyDBG(end3);}

    // Return on succes:
    pthread_exit(NULL);

    // Return on error:
    end3: close(s.srvfd);
    end2: free(s.cli);
    end1: free(s.epfd);
    end0: return -1;
}

//-----------------------------------------------------------------------------
// W_Acce:
//-----------------------------------------------------------------------------

void *W_Acce(void *arg)

{
    // Initializations:
    int i;                              // Socket file descriptor.
    struct sockaddr_in cliaddr;         // IPv4 socket address structure.
    socklen_t len = sizeof(cliaddr);    // Fixed length (16 bytes).
    PCLIENT cptr = NULL;                // Pointer to client data.
    struct epoll_event ev;              // Epoll event structure.

    // Setup epoll behavior as one-shot-edge-triggered:
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    // Main thread loop:
    while(1)

    {
        // Initialize the client data structure:
        if((cptr = malloc(sizeof(CLIENT))) == NULL) MyDBG(end0);
        cptr->epfd = (int)arg;

        // Blocking accept returns a non-blocking client socket:
        if((cptr->clifd = accept(s.srvfd, (struct sockaddr *) &cliaddr, &len)) < 0) MyDBG(end1);
        if((i = fcntl(cptr->clifd, F_GETFL)) < 0) MyDBG(end2);
        i |= O_NONBLOCK; if(fcntl(cptr->clifd, F_SETFL, i) < 0) MyDBG(end2);

        // Return data to us later:
        ev.data.ptr = (void *)cptr;

        // Epoll assignment:
        if(epoll_ctl((int)arg, EPOLL_CTL_ADD, cptr->clifd, &ev) < 0) MyDBG(end2);
        continue;
    }

    // Return on error:
    end2: close(cptr->clifd);
    end1: free(cptr);
    end0: pthread_exit(NULL);
}

//-----------------------------------------------------------------------------
// W_Wait:
//-----------------------------------------------------------------------------

void *W_Wait(void *arg)

{
    // Initializations:
    int i, n;                            // For general use.
    struct epoll_event ev[s.cnf.maxe];   // Epoll-events array (C99).

    // Main thread loop:
    while(1)

    {
        // Wait up to s.cnf.maxe on the epoll-set:
        if((n = epoll_wait((int)arg, &ev[0], s.cnf.maxe, -1)) < 0) MyDBG(end0);

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
    PCLIENT cptr = NULL;      // Pointer to client data.
    char buff[MTU];           // Will store RX data.
    int n, len;               // For general use.
    struct epoll_event ev;    // Epoll event structure.

    // Setup epoll behavior as one-shot-edge-triggered:
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    // Main thread loop:
    while(1)

    {
        // Critical section:
        if(pthread_mutex_lock(&mtx) != 0) MyDBG(end0);
        while(s.iget == s.iput){if(pthread_cond_wait(&cnd, &mtx) != 0) MyDBG(end0);}
        cptr = s.cli[s.iget]; if(++s.iget == s.cnf.maxc){s.iget=0;}
        if(pthread_mutex_unlock(&mtx) != 0) MyDBG(end0);

        // Try to non-blocking read some data until it would block or MTU:
        len = 0; read: n = read(cptr->clifd, &buff, MTU-len);
        if(n>0){len+=n; if(parser(buff, n, cptr) < 0){MyDBG(end0);} goto read;}

        // Ok, it would block or enough data readed for this round:
        else if((n<0 && errno==EAGAIN) || (n==0 && len==MTU))

        {
            // Re-arm the trigger:
            ev.data.ptr = (void *)cptr;
            if(epoll_ctl(cptr->epfd, EPOLL_CTL_MOD, cptr->clifd, &ev) < 0) MyDBG(end0);
        }

        // The call was interrupted by a signal before any data was read:
        else if(n<0 && errno==EINTR) goto read;

        // End of connection:
        else if(n<0)

        {
            if(epoll_ctl(cptr->epfd, EPOLL_CTL_DEL, cptr->clifd, NULL) < 0) MyDBG(end0);
            close(cptr->clifd);
            free(cptr);
        }
    }

    // Return on error:
    end0: pthread_exit(NULL);
}

//-----------------------------------------------------------------------------
// parser:
//-----------------------------------------------------------------------------

int parser(char *buff, int len, PCLIENT cptr)

{
    printf("[%d] %d bytes of data.\n", cptr->clifd, len);
    return 0;
}
