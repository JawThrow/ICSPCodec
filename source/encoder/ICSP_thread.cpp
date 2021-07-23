#include "ICSP_thread.h"
#include "ICSP_Codec_Encoder.h"
#include <stdlib.h>

void thread_pool_init(thread_pool_t* pool, int nthreads)
{
    pool = (thread_pool_t*) malloc(sizeof(thread_pool_t));

    if(pool == NULL)
    {
        print_error_message(FAIL_MEM_ALLOC, "thread_pool_init");
    }

    pool->nthreads = nthreads;
    pool->p = (thread_t*)malloc(sizeof(thread_t)*nthreads);

    if(pool->p == NULL)
    {
        print_error_message(FAIL_MEM_ALLOC, "thread_pool_init");
    }
}

void thread_pool_end(thread_pool_t* pool)
{
    if(pool->p != NULL)
    {
        for(int i=0; i<pool->nthreads; i++)
            free(&pool->p[i]);
    }

    if(pool->p != NULL)
    {
        free(pool);
    }
}