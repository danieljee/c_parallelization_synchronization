#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

typedef struct {
    pthread_t thread_id;
    bool joinable;
} thread;

typedef struct Task{
    void (*work)();
    struct Task* next;
    void *args;
} Task;

typedef struct {
    Task* head;
    Task* tail;
} Task_queue;

typedef struct {
    int task_count;
    Task_queue queue;
    int nthreads;
    bool is_finished;
    thread *threads;
    pthread_mutex_t task_mutex;
    pthread_cond_t conditional_var;
} Thread_pool;

typedef struct {
    Thread_pool* t_pool;
    Task* task;
} thread_args;

void terminate(Thread_pool*);

void submit_task(Thread_pool *, void (*)(void *), void*);

Task* get_task(Thread_pool*);

void* default_thread_routine (void *);

Thread_pool* create_thread_pool(int);