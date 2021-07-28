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
    pool->thread_list = (thread_t*)malloc(sizeof(thread_t)*nthreads);

    if(pool->thread_list == NULL)
    {
        print_error_message(FAIL_MEM_ALLOC, "thread_pool_init");
    }    
}

void thread_pool_end(thread_pool_t* pool)
{
    if(pool != NULL)
    {
        if(pool->thread_list != NULL)
        {
            for(int i=0; i<pool->nthreads; i++)
                free(&pool->thread_list[i]);
        }
        free(pool);
    }    
}

void thread_pool_start(thread_pool_t* pool, int nthreads, cmd_options_t* opt)
{
    // create job queue
    int intra_period = opt->intra_period;
    int total_jobs = opt->total_frames / intra_period;
    int QP_DC = opt->QP_DC;
    int QP_AC = opt->QP_AC;
    for(int njobs=0; njobs<total_jobs; njobs++)
    {
        encoding_jobs_t jobs;
        jobs.start_frame_num = njobs * intra_period;
        jobs.end_frame_num = jobs.start_frame_num + intra_period - 1;
        jobs.QP_DC = QP_DC;
        jobs.QP_AC = QP_AC;
        // frames in global or 
    }

    // create pthread
    for(int i=0; i<nthreads; i++)
    {
        
    }
}