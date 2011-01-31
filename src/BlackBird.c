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

//-----------------------------------------------------------------------------
// Typedefs:
//-----------------------------------------------------------------------------

typedef struct _CORE_STRC

{
    int epfd;
    pthread_t tid;
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
#define DESCRIPTORS_HINT 100

//-----------------------------------------------------------------------------
// Entry point:
//-----------------------------------------------------------------------------

int main(int argc, char *argv[])

{
    // Initializations:
    int i, j = sysconf(_SC_NPROCESSORS_ONLN);   // Number of cores.
    pthread_t thread = pthread_self();          // Main thread ID (myself).
    cpu_set_t cpuset;                           // Each bit represents a CPU.
    CORE_STRC core[j];                          // Variable-length array (C99).

    // For each core in the system:
    for(i=0; i<j; i++)

    {
        // Open an epoll file descriptor by requesting the kernel to allocate an
        // event backing store dimensioned for DESCRIPTORS_HINT descriptors:
        if((core[i].epfd = epoll_create(DESCRIPTORS_HINT)) < 0) MyDBG(end0);

        // New threads inherits a copy of its creator's CPU affinity mask:
        CPU_ZERO(&cpuset); CPU_SET(i, &cpuset);
        if(pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end0);
        if(pthread_create(&core[i].tid, NULL, Worker, (void *) &core[i]) != 0) MyDBG(end0);
    }

    // Restore parent affinity to all available cores:
    CPU_ZERO(&cpuset); for(i=0; i<j; i++) {CPU_SET(i, &cpuset);}
    if(pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) MyDBG(end0);

    // Print info about myself:
    sleep(1);
    pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    printf("Thread:%u ", (unsigned int)pthread_self());
    for (i=0; i<CPU_SETSIZE; i++) {if (CPU_ISSET(i, &cpuset)) printf("CPU:%d ", i);}
    printf("\n");

    // Return on success:
    pthread_exit(NULL);

    // Return on error:
    end0: return -1;
}

//-----------------------------------------------------------------------------
// Worker:
//-----------------------------------------------------------------------------

void *Worker(void *arg)

{
    // Initializations:
    CORE_STRC *core = (PCORE_STRC) arg;
    cpu_set_t cpuset;
    int i;

    // Print info about myself:
    sleep(2);
    pthread_getaffinity_np(core->tid, sizeof(cpu_set_t), &cpuset);
    printf("Thread:%u Epoll:%d ", (unsigned int)pthread_self(), core->epfd);
    for (i=0; i<CPU_SETSIZE; i++) {if (CPU_ISSET(i, &cpuset)) printf("CPU:%d\n", i);}

    // Return on success:
    pthread_exit(NULL);
}
