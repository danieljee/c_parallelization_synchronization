#include "thread_pool.h"

void terminate(Thread_pool* t_pool)
{
    printf("Terminating thread pool of %d threads----------\n", t_pool->nthreads);
    for (int i=0; i != t_pool->nthreads; ++i)
    {   
        printf("-- Cancelling thread %ld\n", t_pool->threads[i].thread_id);   
        pthread_cancel(t_pool->threads[i].thread_id); 
        printf("-- Thread %ld terminated\n", t_pool->threads[i].thread_id);
    }
    printf("-- All threads terminated\n");
     
    //These will fail if locks are being used.
    pthread_mutex_destroy(&t_pool->task_mutex);
    pthread_cond_destroy(&t_pool->conditional_var);
    printf("-- Locks destroyed\n");
    if (t_pool->threads != NULL) free(t_pool->threads);
    if (t_pool != NULL) free(t_pool);
    printf("-- Memory freed\n");
}

void submit_task(Thread_pool *t_pool, void (*work)(void *), void* args)
{
    Task *task = malloc(sizeof(Task));
    if (!task)
    {
        fprintf(stderr, "Failed to allocate task memory\n");
        exit(EXIT_FAILURE);
    }
    
    task->work = work;
    task->args = args;
    task->next = NULL;
    pthread_mutex_lock(&t_pool->task_mutex);
    if (t_pool->task_count == 0)
    {
        t_pool->queue.head = task;
        t_pool->queue.tail = task;
    } else {
        t_pool->queue.tail->next = task;
    }
    ++t_pool->task_count;
    pthread_mutex_unlock(&t_pool->task_mutex);
    pthread_cond_signal(&t_pool->conditional_var);
}

Task* get_task(Thread_pool* t_pool)
{
    Task_queue queue = t_pool->queue;
    Task* ret = queue.head;
    queue.head = ret->next;
    --t_pool->task_count;
    ret->next = NULL;

    return ret;
}

void default_clean_up(void *_args)
{
    printf("-- default cleanup\n");
    thread_args *args = (thread_args *)_args;
    if (args != NULL && args->task != NULL) free(args->task);
    if (args != NULL) free(args);
    printf("-- default cleanup finished\n");
}

void* default_thread_routine (void *_args)
{
    pthread_cleanup_push(default_clean_up, _args);  

    thread_args *args = (thread_args *)_args;
    Thread_pool *t_pool = args->t_pool;
    while(!t_pool->is_finished)
    {
        pthread_mutex_lock(&t_pool->task_mutex);
        if (t_pool->task_count == 0)
        {
            pthread_cond_wait(&t_pool->conditional_var, &t_pool->task_mutex);
        }
        Task *task = get_task(t_pool);
        args->task = task;
        pthread_mutex_unlock(&t_pool->task_mutex);
        // Execute.
        (*task->work)(task->args);
        free(task);
    }
    //Finished work. Clean up.
    pthread_cleanup_pop(1);
    printf("Thread(%ld): terminating!\n", pthread_self());
}

Thread_pool* create_thread_pool(int nthreads)
{
    Thread_pool *t_pool = (Thread_pool*) calloc(1, sizeof(Thread_pool));
    t_pool->task_count = 0;
    t_pool->nthreads = nthreads;
    t_pool->is_finished = false;
    t_pool->threads = (thread*) malloc(nthreads * sizeof(thread));

    pthread_mutex_init(&t_pool->task_mutex, NULL);
    pthread_cond_init(&t_pool->conditional_var, NULL);

    printf("Creating thread pool ----------\n");

    for (int i=0; i != nthreads; ++i)
    {
        thread_args *args = (thread_args *) malloc(sizeof(Thread_pool));
        args->t_pool = t_pool;
        if (pthread_create(&t_pool->threads[i].thread_id, NULL, default_thread_routine, args) != 0) {
            terminate(t_pool);
            printf("Failed to create thread\n");
            return NULL;
        }
        printf("-- Created thread %ld\n", t_pool->threads[i].thread_id);
        t_pool->threads[i].joinable = true;
    }
    return t_pool;
}