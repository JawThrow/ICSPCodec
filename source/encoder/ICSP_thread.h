#ifndef ICSP_THREAD
#define ICSP_THREAD

#include <pthread.h>

typedef struct 
{
	pthread_t pid;
	int thread_idx; // index in thread_list
    void *(*encoding_function)(void*);
    void *arg;
}thread_t;

typedef struct 
{
    thread_t* thread_list;
    int nthreads;
}thread_pool_t;

void thread_pool_init(thread_pool_t* pool, int nthreads);
void thread_pool_end(thread_pool_t* pool);

typedef struct 
{
	FrameData* pFrames;
	int nFrames;
	int start_frame_num;
	int end_frame_num;
	int QP_DC;
	int QP_AC;
}encoding_jobs_t;

#endif
