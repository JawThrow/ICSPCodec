#ifndef ICSP_THREAD
#define ICSP_THREAD

#include <pthread.h>

typedef struct 
{
    void *(*encoding_function)(void*);
    void *arg;
}thread_t;

typedef struct 
{
    thread_t* p;
    int nthreads;
}thread_pool_t;

void thread_pool_init(thread_pool_t* pool, int nthreads);
void thread_pool_end(thread_pool_t* pool);

#endif
