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

#include "bb_main.h"

//-----------------------------------------------------------------------------
// Globals:
//-----------------------------------------------------------------------------

SERVER s;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cnd = PTHREAD_COND_INITIALIZER;

//-----------------------------------------------------------------------------
// parser:
//-----------------------------------------------------------------------------

int parser(char *buff, int len, PCLIENT cptr)

{
    int i;
    char resp[] = { 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 
                    0x20, 0x32, 0x30, 0x30, 0x20, 0x4f, 0x4b, 0x0d, 
                    0x0a, 0x44, 0x61, 0x74, 0x65, 0x3a, 0x20, 0x54, 
                    0x68, 0x75, 0x2c, 0x20, 0x32, 0x34, 0x20, 0x46, 
                    0x65, 0x62, 0x20, 0x32, 0x30, 0x31, 0x31, 0x20, 
                    0x31, 0x35, 0x3a, 0x35, 0x31, 0x3a, 0x34, 0x31, 
                    0x20, 0x47, 0x4d, 0x54, 0x0d, 0x0a, 0x53, 0x65, 
                    0x72, 0x76, 0x65, 0x72, 0x3a, 0x20, 0x41, 0x70, 
                    0x61, 0x63, 0x68, 0x65, 0x0d, 0x0a, 0x4c, 0x61, 
                    0x73, 0x74, 0x2d, 0x4d, 0x6f, 0x64, 0x69, 0x66, 
                    0x69, 0x65, 0x64, 0x3a, 0x20, 0x4d, 0x6f, 0x6e, 
                    0x2c, 0x20, 0x31, 0x33, 0x20, 0x41, 0x75, 0x67, 
                    0x20, 0x32, 0x30, 0x30, 0x37, 0x20, 0x31, 0x38, 
                    0x3a, 0x34, 0x38, 0x3a, 0x33, 0x31, 0x20, 0x47, 
                    0x4d, 0x54, 0x0d, 0x0a, 0x45, 0x54, 0x61, 0x67, 
                    0x3a, 0x20, 0x22, 0x31, 0x63, 0x37, 0x38, 0x30, 
                    0x33, 0x37, 0x2d, 0x62, 0x2d, 0x32, 0x62, 0x63, 
                    0x39, 0x39, 0x64, 0x63, 0x30, 0x22, 0x0d, 0x0a, 
                    0x41, 0x63, 0x63, 0x65, 0x70, 0x74, 0x2d, 0x52, 
                    0x61, 0x6e, 0x67, 0x65, 0x73, 0x3a, 0x20, 0x62, 
                    0x79, 0x74, 0x65, 0x73, 0x0d, 0x0a, 0x43, 0x6f, 
                    0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x4c, 0x65, 
                    0x6e, 0x67, 0x74, 0x68, 0x3a, 0x20, 0x31, 0x31, 
                    0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 
                    0x74, 0x2d, 0x54, 0x79, 0x70, 0x65, 0x3a, 0x20, 
                    0x74, 0x65, 0x78, 0x74, 0x2f, 0x68, 0x74, 0x6d, 
                    0x6c, 0x3b, 0x20, 0x63, 0x68, 0x61, 0x72, 0x73, 
                    0x65, 0x74, 0x3d, 0x55, 0x54, 0x46, 0x2d, 0x38, 
                    0x0d, 0x0a, 0x0d, 0x0a, 0x48, 0x65, 0x6c, 0x6c, 
                    0x6f, 0x57, 0x6f, 0x72, 0x6c, 0x64, 0x0a };

    for(i=0; i<len; i++)

    {
        if(buff[i]=='\x0d' && buff[i+1]=='\x0a' && buff[i+2]=='\x0d' && buff[i+3]=='\x0a')

        {
            //printf("[%d]\n", cptr->clifd);
            write(cptr->clifd, resp, sizeof(resp));
            break;
        }
    }

    return 0;
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
        while(bb_fifo_empty(&s.fifo)){if(pthread_cond_wait(&cnd, &mtx) != 0) MyDBG(end0);}
        cptr = (PCLIENT)s.fifo.cap->nxt->cptr; bb_fifo_pop(&s.fifo);
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

        // Server has closed the socket:
        else if(n<0 && errno==EBADF) free(cptr);

        // Client has terminated:
        else {close(cptr->clifd); free(cptr);}
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
    int i, n;                             // For general use.
    struct epoll_event ev[s.cnf.epoev];   // Epoll-events array (C99).

    // Main thread loop:
    while(1)

    {
        // Wait up to s.cnf.epoev on the epoll-set:
        wait: n = epoll_wait((intptr_t)arg, &ev[0], s.cnf.epoev, -1);
        if(n<0){if(errno==EINTR){goto wait;} else{MyDBG(end0);}}

        // For each event fired: if the fd is available to be read from
        // without blocking, it is transfered to the Data Workers pool:
        for(i=0; i<n; i++)

        {
            if(ev[i].events & EPOLLIN)

            {
                // Enter the critical section:
                if(pthread_mutex_lock(&mtx) != 0) MyDBG(end0);

                // Push the client-data pointer:
                if(bb_fifo_push(&s.fifo, ev[i].data.ptr) < 0) MyDBG(end0);

                // Signal to awake sleeping thread:
                if(pthread_cond_signal(&cnd) != 0) MyDBG(end0);

                // Leave the critical section:
                if(pthread_mutex_unlock(&mtx) != 0) MyDBG(end0);
            }

            else {printf("Event is not: EPOLLIN\n");}
        }
    }

    // Return on error:
    end0: pthread_exit(NULL);
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
        cptr->epfd = (intptr_t)arg;

        // Blocking accept returns a non-blocking client socket:
        if((cptr->clifd = accept(s.srvfd, (struct sockaddr *) &cliaddr, &len)) < 0) MyDBG(end1);
        if(s.cnf.tcpnd){i=1; if(setsockopt(cptr->clifd, SOL_TCP, TCP_NODELAY, &i, sizeof(i)) < 0) MyDBG(end2);}
        if((i = fcntl(cptr->clifd, F_GETFL)) < 0) MyDBG(end2);
        i |= O_NONBLOCK; if(fcntl(cptr->clifd, F_SETFL, i) < 0) MyDBG(end2);

        // Return data to us later:
        ev.data.ptr = (void *)cptr;

        // Epoll assignment:
        if(epoll_ctl((intptr_t)arg, EPOLL_CTL_ADD, cptr->clifd, &ev) < 0) MyDBG(end2);
        continue;
    }

    // Return on error:
    end2: close(cptr->clifd);
    end1: free(cptr);
    end0: pthread_exit(NULL);
}

//-----------------------------------------------------------------------------
// sig_int:
//-----------------------------------------------------------------------------

void sig_int(int signo)

{
    exit(EXIT_SUCCESS);
}

//-----------------------------------------------------------------------------
// Entry point:
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])

{
    // Initializations:
    int i, j;                      // For general use.
    pthread_t thread;              // Main thread ID (myself).
    cpu_set_t cpuset;              // Each bit represents a CPU.
    struct sockaddr_in srvaddr;    // IPv4 socket address structure.

    // Set config defaults:
    s.cnf.ehint = EPOLL_HINT;
    s.cnf.epoev = EPOLL_EVENTS;
    s.cnf.athre = ACCEPT_THREADS;
    s.cnf.dthre = DATA_THREADS;
    s.cnf.tcpnd = TCP_NDELAY;

    // Parse command line options:
    struct option longopts[] = {
    { "epoll-hint",     required_argument,  NULL,  'h' },
    { "epoll-events",   required_argument,  NULL,  'e' },
    { "accept-threads", required_argument,  NULL,  'a' },
    { "data-threads",   required_argument,  NULL,  'd' },
    { "tcp-nodelay",    no_argument,        NULL,  'n' },
    { 0, 0, 0, 0 }};

    while((i = getopt_long(argc, argv, "h:e:a:d:n", longopts, NULL)) != -1)

    {
        if (i == -1) break;

        switch(i)

        {
            case 'h': s.cnf.ehint = atoi(optarg);
                      break;
            case 'e': s.cnf.epoev = atoi(optarg);
                      break;
            case 'a': s.cnf.athre = atoi(optarg);
                      break;
            case 'd': s.cnf.dthre = atoi(optarg);
                      break;
            case 'n': s.cnf.tcpnd = 1;
                      break;
            default:  abort();
        }
    }

    // Daemonize:
    daemonize();

    // Initialize server structures:
    s.cores = sysconf(_SC_NPROCESSORS_ONLN);
    if((s.epfd = malloc(sizeof(int) * s.cores)) == NULL) MyDBG(end0);
    if(bb_fifo_new(&s.fifo) < 0) MyDBG(end1);

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

    // For each core in the system:
    for(i=0; i<s.cores; i++)

    {
        // Open an epoll fd dimensioned for s.cnf.ehint/s.cores descriptors:
        if((s.epfd[i] = epoll_create(s.cnf.ehint/s.cores)) < 0) MyDBG(end2);

        // Wait-Worker inherits a copy of its creator's CPU affinity mask:
        CPU_ZERO(&cpuset); CPU_SET(i, &cpuset);
        if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end2);
        if(pthread_create(&thread, NULL, W_Wait, (void *)(intptr_t)s.epfd[i]) != 0) MyDBG(end2);
    }

    // Round-Robin distribution of Accept-Workers among all available cores:
    for (j=0; j<s.cnf.athre; j++){for (i=0; i<s.cores; i++)

    {
        // Accept-Worker inherits a copy of its creator's CPU affinity mask:
        CPU_ZERO(&cpuset); CPU_SET(i, &cpuset);
        if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end2);
        if(pthread_create(&thread, NULL, W_Acce, (void *)(intptr_t)s.epfd[i]) != 0) MyDBG(end2);
    }}

    // Restore creator's (myself) affinity to all available cores:
    CPU_ZERO(&cpuset); for(i=0; i<s.cores; i++){CPU_SET(i, &cpuset);}
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end2);

    // Pre-threading a pool of s.cnf.dthre Data-Workers:
    for(i=0; i<s.cnf.dthre; i++){if(pthread_create(&thread, NULL, W_Data, NULL) != 0) MyDBG(end2);}

    // Register a signal handler for SIGINT (Ctrl-C)
    if((signal(SIGINT, sig_int)) == SIG_ERR) MyDBG(end2);

    // Loop forever:
    while(1){sleep(10);}

    // Return on error:
    end2: close(s.srvfd);
    end1: free(s.epfd);
    end0: return -1;
}
