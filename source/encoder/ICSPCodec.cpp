#include "ICSP_Codec_Encoder.h"
#include "ICSP_thread.h"

void IcspCodec::init(int nframe, char* imageFname, int width, int height, int QstepDC, int QstepAC)
{
	if(YCbCrLoad(*this, imageFname, nframe, width, height)!=0)
	{
		cout << " error from YCbCrLoad" << endl;
		exit(-1);
	}

	if(splitFrames(*this)!=0)
	{
		cout << " error from splitFrames" << endl;
		exit(-1);
	}

	free(YCbCr.Ys);
	free(YCbCr.Cbs);
	free(YCbCr.Crs);
	
	if(splitBlocks(*this, 16, 8)!=0)
	{
		cout << "error from splitBlocks" << endl;
		exit(-1);
	}
	
	this->QstepDC = QstepDC;
	this->QstepAC = QstepAC;
}
	
void IcspCodec::encoding(cmd_options_t* opt)
{
	
	if(!opt->multi_thread_mode) // single-thread mode
	{
		int intraPeriod = opt->intra_period;
		if( intraPeriod==ALL_INTRA )
		{
			allintraPrediction(frames, YCbCr.nframe, QstepDC, QstepAC);
			makebitstream(frames, YCbCr.nframe, YCbCr.height, YCbCr.width, QstepDC, QstepAC, intraPeriod, INTRA);
			checkResultFrames(frames, YCbCr.width, YCbCr.height, YCbCr.nframe, INTRA, SAVE_YUV);
		}
		else
		{
			for(int n=0; n<YCbCr.nframe; n++)
			{
				int frame_type = 0;			
				if(n%intraPeriod==0)
				{
					frame_type = I_FRAME;
					intraPrediction(frames[n], QstepDC, QstepAC);				
				}
				else
				{
					frame_type = P_FRAME;
					interPrediction(frames[n], frames[n-1], QstepDC, QstepAC);
				}
				print_frame_end_message(n, frame_type);
			}		
			makebitstream(frames, YCbCr.nframe, YCbCr.height, YCbCr.width, QstepDC, QstepAC, intraPeriod, INTER);
			checkResultFrames(frames, YCbCr.width, YCbCr.height,YCbCr.nframe, INTER, SAVE_YUV);
		}
	}
	else // multi-thread mode
	{		
		thread_pool_t* pool;
		thread_pool_init(pool, opt->nthreads);
		thread_pool_start(pool, opt->nthreads, frames, opt);
		thread_pool_end(pool);
	}

	
	
}

IcspCodec::~IcspCodec()
{
	// YCbCr unsigned pointer free; frames free; frames -> blocks free
	// YCbCr�� �ҷ��� ��ü ���� free

	for(int i=0; i<YCbCr.nframe; i++)
	{
		free(frames[i].blocks);
		free(frames[i].Cbblocks);
		free(frames[i].Crblocks);
	}		
}