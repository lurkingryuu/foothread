/**
 * @file foothread.c
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

#include <foothread.h>
#include <stdio.h> // debug

// ------------------------------------ Threads ------------------------------------
void foothread_attr_setjointype(foothread_attr_t *attr, int join_type)
{
    attr->join_type = join_type;
}
void foothread_attr_setstacksize(foothread_attr_t *attr, int stack_size)
{
    attr->stack_size = stack_size;
}

foothread_attr_t attr = FOOTHREAD_ATTR_INITIALIZER;

static foothread_t threads[FOOTHREAD_THREADS_MAX];
static int thread_count = 0;
static int initialised = 0;

static int key = 100;

// mutex
int mutex = 0;
struct sembuf pop = {0, -1, 0};
struct sembuf vop = {0, 1, 0};

// semaphores for join
int join_sem = 0;

// signal handler
void sig_handle(int signum)
{
    // destroy semaphores
    semctl(mutex, 0, IPC_RMID, 0);
    semctl(join_sem, 0, IPC_RMID, 0);
}

/**
 * @brief Initialize the library
 * 
 * @return int
*/
int init()
{
    if (initialised)
        return 0;
    mutex = semget(key++, 1, 0666 | IPC_CREAT);
    if (mutex < 0)
    {
        perror("semget failed mutex");
        return -1;
    }
    semctl(mutex, 0, SETVAL, 1);

    join_sem = semget(key++, 1, 0666 | IPC_CREAT);
    if (join_sem < 0)
    {
        perror("semget failed join_sem");
        return -1;
    }
    semctl(join_sem, 0, SETVAL, 0);

    signal(SIGTERM, sig_handle);
    initialised = 1;
    return 1;
}


/**
 * @brief Create a new thread
 * 
 * @param thread - pointer to thread object 
 * @param attr - pointer to thread attributes
 * @param start_routine - pointer to function to be executed by the thread
 * @param arg - void pointer to arguments to be passed to the function
 * @return int
 */
int foothread_create(foothread_t *thread, foothread_attr_t *attr, int (*start_routine)(void *), void *arg)
{

    init();

    semop(mutex, &pop, 1); // wait on mutex

    if (thread_count >= FOOTHREAD_THREADS_MAX)
    {
        perror("max threads reached");
        semop(mutex, &vop, 1); // signal on mutex
        return -1;
    }

    // initialize variables
    int thread_idx = thread_count;
    thread_count++;
    threads[thread_idx].id = thread_count;
    threads[thread_idx].leader_id = gettid();
    threads[thread_idx].state = 0;
    threads[thread_idx].start_routine = start_routine;
    threads[thread_idx].arg = arg;

    // allocate stack
    threads[thread_idx].stack = (void *)malloc(attr->stack_size);
    if (threads[thread_idx].stack == NULL)
    {
        perror("malloc stack failed");
        semop(mutex, &vop, 1); // signal on mutex
        return -1;
    }

    // Default attributes
    if (attr == NULL)
    {
        threads[thread_idx].attr.join_type = FOOTHREAD_DETACHED;
        threads[thread_idx].attr.stack_size = FOOTHREAD_DEFAULT_STACK_SIZE;
    }
    else
    {
        threads[thread_idx].attr = *attr;
    }

    // Create thread
    int ret = clone(
        start_routine,
        threads[thread_idx].stack + threads[thread_idx].attr.stack_size,
        FOOTHREAD_CLONE_FLAGS,
        threads[thread_idx].arg);

    if (ret == -1)
    {
        perror("clone failed");
        // reset to default
        memset(&threads[thread_idx], 0, sizeof(foothread_t));
        thread_count--;
        free(threads[thread_idx].stack);

        semop(mutex, &vop, 1); // signal on mutex
        return -1;
    }

    // Detached thread
    if (threads[thread_idx].attr.join_type == FOOTHREAD_DETACHED)
    {
        threads[thread_idx].leader_id = threads[thread_idx].id;
    }

    semop(mutex, &vop, 1); // signal on mutex

    return 0;
}

void foothread_exit()
{
    if (!initialised)
        return;

    // identify self index
    int i, tid = gettid(), r;
    int self_idx = -1;
    for (i = 0; i < FOOTHREAD_THREADS_MAX; i++)
    {
        if (threads[i].id == tid)
        {
            self_idx = i;
            break;
        }
    }
    if (threads[self_idx].attr.join_type == FOOTHREAD_DETACHED)
    {
        return;
    }
    int is_leader = 0;
    for (i = 0; i < FOOTHREAD_THREADS_MAX; i++)
    {
        if (threads[i].leader_id == tid)
        {
            is_leader = 1;
            break;
        }
    }

    // wait for all joinable followers
    if (is_leader)
    {
        for (i = 0; i < thread_count; i++)
        {
            if (threads[i].leader_id == tid)
            {
                // printf("[%d] waiting on follower: %d\n", tid, threads[i].id); // debug
                r = semop(join_sem, &pop, 1); // wait on join_sem
                if (r<0) {
                    perror("wait on join_sem");
                    return;
                }
            }

        }
        // destroy semaphores
        semctl(mutex, 0, IPC_RMID, 0);
        mutex = -1;
        semctl(join_sem, 0, IPC_RMID, 0);
        join_sem = -1;
        initialised = 0;
        return;
    }
    else // follower
    {
        // printf("signalling from thread: %d\n",tid); // debug
        semop(join_sem, &vop, 1); // signal on join_sem
        return;
    }
}


// ------------------------------------ Mutex ------------------------------------

void foothread_mutex_init(foothread_mutex_t *mutex)
{
    mutex->mtx = semget(key++, 1, 0666 | IPC_CREAT);
    mutex->owner = gettid();
}

void foothread_mutex_lock(foothread_mutex_t *mutex)
{
    struct sembuf pop = {0, -1, 0};
    semop(mutex->mtx, &pop, 1);
    mutex->owner = gettid();   
}

void foothread_mutex_unlock(foothread_mutex_t *mutex)
{
    struct sembuf vop = {0, 1, 0};
    if (mutex->owner != gettid())
    {
        perror("mutex unlock: not owner");
        exit(EXIT_FAILURE);
    }
    if (semctl(mutex->mtx, 0, GETVAL, 0) == 1)
    {
        perror("mutex unlock: already unlocked");
        exit(EXIT_FAILURE);
    }
    semop(mutex->mtx, &vop, 1);
}

void foothread_mutex_destroy(foothread_mutex_t *mutex)
{
    semctl(mutex->mtx, 0, IPC_RMID, 0);
    mutex->mtx = -1;
    mutex->owner = -1;
}

// ------------------------------------ Barrier ------------------------------------

void foothread_barrier_init(foothread_barrier_t *barrier, int n)
{
    barrier->count = 0;
    barrier->n = n;
    barrier->sem = semget(key++, 1, 0666 | IPC_CREAT);
    semctl(barrier->sem, 0, SETVAL, 0);
}

void foothread_barrier_wait(foothread_barrier_t *barrier)
{
    struct sembuf pop = {0, -1, 0};
    struct sembuf vop = {0, 1, 0};
    barrier->count++;
    if (barrier->count >= barrier->n)
    {
        for (int i = 0; i < barrier->n; i++)
        {
            semop(barrier->sem, &vop, 1);
        }

        // // reset barrier
        // barrier->count = 0;
    }
    if (barrier->count > barrier->n)
    {
        return;
    }
    semop(barrier->sem, &pop, 1);
}

void foothread_barrier_destroy(foothread_barrier_t *barrier)
{
    if (barrier->sem < 0)
    {
        return;
    }
    semctl(barrier->sem, 0, IPC_RMID, 0);
    barrier->count = 0;
    barrier->n = 0;
    barrier->sem = -1;
}
