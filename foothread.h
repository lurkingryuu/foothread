/**
 * @file foothread.h
 * @author Yelisetty Karthikeya S M (21CS30060)
 * @brief Implementation of the foothread library, a user-level thread library
 * 
 * This file contains the implementation of the foothread library, a user-level thread library
 * 
 * The library provides the following functionalities:
 * - Thread creation
 * - Thread exit
 * - Mutex
 * - Barrier
 * 
 * The library is implemented using the clone system call, which creates a new process (thread) in the same address space as the calling process.
 * 
 * @date 2021-03-31
 * @copyright MIT License
*/

#ifndef _FOOTHREAD_H
#define _FOOTHREAD_H

#define _GNU_SOURCE
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define gettid() syscall(SYS_gettid)


#define FOOTHREAD_CLONE_FLAGS (\
    CLONE_THREAD\
    | CLONE_SIGHAND\
    | CLONE_VM\
    | CLONE_FS\
    )
    /*| CLONE_PARENT \*/

#define FOOTHREAD_THREADS_MAX 1024
#define FOOTHREAD_DEFAULT_STACK_SIZE 2*1024*1024

#define FOOTHREAD_JOINABLE 0
#define FOOTHREAD_DETACHED 1 // default behavior

typedef struct foothread_attr_t {
    int join_type;
    int stack_size;
} foothread_attr_t;

#define FOOTHREAD_ATTR_INITIALIZER {FOOTHREAD_DETACHED, FOOTHREAD_DEFAULT_STACK_SIZE}

void foothread_attr_setjointype(foothread_attr_t *, int);
void foothread_attr_setstacksize(foothread_attr_t *, int);


typedef struct foothread_t {
    int id;
    int leader_id;
    int state;
    foothread_attr_t attr;
    void *stack;
    int (*start_routine)(void *);
    void *arg;
} foothread_t;

int foothread_create(foothread_t *, foothread_attr_t *, int (*)(void *), void *);

void foothread_exit();

// Mutex
/*
Define a data type foothread_mutex_t (to implement binary semaphores). Implement the following functions with the obvious meanings.
       void foothread_mutex_init ( foothread_mutex_t * ) ;
       void foothread_mutex_lock ( foothread_mutex_t * ) ;
       void foothread_mutex_unlock ( foothread_mutex_t * ) ;
       void foothread_mutex_destroy ( foothread_mutex_t * ) ;
Unlike Linux semaphores, foothread mutexes need to satisfy the following restrictions.
1. Only the thread that locks a mutex can unlock it. An attempt to unlock by another thread will lead to an error.
2. A locked mutex can be attempted to be locked. But then, the new requester will be blocked until the mutex is available to it for relocking.
3. An attempt to unlock an unlocked mutex should lead to an error
*/

typedef struct foothread_mutex_t {
    int mtx;
    int owner;
} foothread_mutex_t;

void foothread_mutex_init ( foothread_mutex_t * ) ;
void foothread_mutex_lock ( foothread_mutex_t * ) ;
void foothread_mutex_unlock ( foothread_mutex_t * ) ;
void foothread_mutex_destroy ( foothread_mutex_t * ) ;


/*

Barriers
Define a data type foothread_barrier_t and the following functions.
       void foothread_barrier_init ( foothread_barrier_t * , int ) ;
       void foothread_barrier_wait ( foothread_barrier_t * ) ;
       void foothread_barrier_destroy ( foothread_barrier_t * ) ;
A foothread barrier must not be used without initialization.
*/

typedef struct foothread_barrier_t {
    int count;
    int n;
    int sem;
} foothread_barrier_t;

void foothread_barrier_init ( foothread_barrier_t * , int ) ;
void foothread_barrier_wait ( foothread_barrier_t * ) ;
void foothread_barrier_destroy ( foothread_barrier_t * ) ;


#endif // _FOOTHREAD_H