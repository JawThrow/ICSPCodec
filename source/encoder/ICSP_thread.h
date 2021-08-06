#ifndef ICSP_THREAD
#define ICSP_THREAD
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include "ICSP_Codec_Encoder.h"
using namespace std;

typedef struct 
{
	FrameData* pFrames;
	int nFrames;
	int start_frame_num;
	int end_frame_num;
	int QP_DC;
	int QP_AC;
}encoding_jobs_t;

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
	queue<encoding_jobs_t> job_queue;
	pthread_mutex_t pool_mutex;	
}thread_pool_t;

void thread_pool_init(thread_pool_t* pool, int nthreads);
void thread_pool_end(thread_pool_t* pool);
void thread_pool_start(thread_pool_t* pool, int nthreads, FrameData* frames, cmd_options_t* opt);

#endif
