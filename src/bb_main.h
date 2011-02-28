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
#include "bb_fifo.h"

//-----------------------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------------------

#define MyDBG(x) do {printf("(%d) %s:%d\n", errno, __FILE__, __LINE__); goto x;} while (0)

#define EPOLL_HINT 500     // Defaults for ehint.
#define EPOLL_EVENTS 10    // Defaults for epoev.
#define ACCEPT_THREADS 2   // Defaults for athre.
#define DATA_THREADS 20    // Defaults for dthre.
#define TCP_NDELAY 0       // Defaukts for tcpnd.
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
    int ehint;   // Epoll size hint.
    int epoev;   // Max epoll events per round.
    int athre;   // Accept-threads per core.
    int dthre;   // Data-threads pool size.
    int tcpnd;   // Control the Nagle algorithm.
}

CONF, *PCONF;

typedef struct _SERVER

{
    int srvfd;     // Server socket file descriptor.
    int cores;     // Number of system cores.
    int *epfd;     // Will point to an epfd array.
    CONF cnf;      // Will store configuration options.
    FIFO fifo;     // This FIFO will store PCLIENTs.
}

SERVER, *PSERVER;

//-----------------------------------------------------------------------------
// End of include guard:
//-----------------------------------------------------------------------------

#endif
