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
	if(opt->multi_thread_mode) // multi-thread mode
	{	
		multi_thread_encoding(opt, frames);			
		
	}
	else // single-thread mode
	{
		single_thread_encoding(frames, &YCbCr, opt->intra_period, QstepDC, QstepAC);
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