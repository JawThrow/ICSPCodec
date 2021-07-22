#ifndef ICSP_THREAD
#define ICSP_THREAD

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

int thread_pool_init(thread_pool_t* pool, int nthreads);
int thread_pool_end(thread_pool_t* pool);

#endif
