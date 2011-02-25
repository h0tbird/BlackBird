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
// Include guard:
//-----------------------------------------------------------------------------

#ifndef _BB_
#define _BB_

//-----------------------------------------------------------------------------
// Includes:
//-----------------------------------------------------------------------------

#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <strings.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

//-----------------------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------------------

#define MyDBG(x) do {printf("(%d) %s:%d\n", errno, __FILE__, __LINE__); goto x;} while (0)

#define MAX_CLIENTS 500    // Defaults for maxc.
#define MAX_THREADS 50     // Defaults for maxt.
#define MAX_EVENTS 50      // Defaults for maxe.
#define TCP_NDELAY 0       // Defaukts for tcpd.
#define LISTENP 8080       // Server listen port.
#define LISTENQ 1024       // sysctl -w net.core.somaxconn=1024
#define MTU 2896           // 2*(1500-40-12) per socket and round.

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
    int maxt;    // Pre-threading pool size.
    int maxe;    // Epoll events per round.
    int tcpd;    // Control the Nagle algorithm.
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
void sig_int(int signo);    // Signal handler.

int parser(char *buff, int len, PCLIENT cptr);

//-----------------------------------------------------------------------------
// End of include guard:
//-----------------------------------------------------------------------------

#endif
