#pragma once
#include <iostream>
#include <time.h>

using namespace std;

#define INTRA    0
#define INTER    1
#define CB       3
#define CR       4
#define SAVE_Y   5
#define SAVE_YUV 6

#define MAXFRAMES 300



const double pi = 3.14159265359;
const double costable[8][8] =
	{1.0,        1.0,        1.0,       1.0,       1.0,       1.0,       1.0,       1.0,
	 0.980785,   0.83147,    0.55557,   0.19509,  -0.19509,  -0.55557,  -0.83147,  -0.980785,
	 0.92388,    0.382683,  -0.382683, -0.92388,  -0.92388,  -0.382683,  0.382683,  0.92388,
	 0.83147,   -0.19509,   -0.980785, -0.55557,   0.55557,   0.980785,  0.19509,  -0.83147,
	 0.707107,  -0.707107,  -0.707107,  0.707107,  0.707107, -0.707107, -0.707107,  0.707107,
	 0.55557,   -0.980785,   0.19509,   0.83147,  -0.83147,  -0.19509,   0.980785, -0.55557,
	 0.382683,  -0.92388,    0.92388,  -0.382683, -0.382683,  0.92388,  -0.92388,   0.382683,
	 0.19509,   -0.55557,    0.83147,  -0.980785,  0.980785, -0.83147,   0.55557,  -0.19509};	// cos table이 올바른 check
const double irt2 = 1.0/sqrt(2.0); // inverse square root 2


struct Block8d{ double block[8][8]; };
struct Block8i{ int block[8][8]; };
struct Block8u{	unsigned char block[8][8];};
struct Block8s{ signed char block[8][8]; };

struct Block16d{ double block[16][16]; };
struct Block16i{ int block[16][16]; };
struct Block16u{ unsigned char block[16][16]; };
struct Block16s{ signed char block[16][16]; };
struct MotionVector{ int x,y;};

#pragma pack(push, 1)	
struct header
{
	char intro[5];
	unsigned short height;
	unsigned short width;
	unsigned char QP_DC;
	unsigned char QP_AC;
	unsigned char DPCMmode;
	unsigned short outro;	
};
#pragma pack(pop)

typedef struct __INTRABODYY
{
	int mpmflag;
	int intraPredictionMode;
	int DCval;
	int ACflag;
	int ACvals[63];
}IntrabodyY;

typedef struct __INTRABODYCBCR
{
	int DCval;
	int ACflag;
	int ACvals[63];
}IntrabodyCbCr;

typedef struct __INTERBODYY
{
	int mvModefalg;
	MotionVector mv;
	int DCval;
	int ACflag;
	int ACvals[63];
}InterbodyY;

typedef struct __INTERBODYCBCR
{
	bool mvModefalg;
	MotionVector mv;
	int DCval;
	int ACflag;
	int ACvals[63];
}InterbodyCbCr;


typedef struct __DBLOCKDATA
{	
	int blocksize1;
	int blocksize2;
	int IDPCMmode[4];
	int MPMFlag[4];
	int intraPredMode[4];
	Block8i **intraQuanblck;
	Block8i **intraInverseQuanblck;
	Block8d **intraInverseDCTblck;
	Block16i *intraInverseErrblck16;
	Block16u intraRestructedblck16;
	Block8u **intraRestructedblck8;
	int* intraReorderedblck[4];

	MotionVector Reconstructedmv;
	int MVmodeflag;
	Block8i **interQuanblck;
	Block8i **interInverseQuanblck;
	Block8d **interInverseDCTblck;
	Block16i *interInverseErrblck16;
	int* interReorderedblck[4];
}DBlockData;

typedef struct __DCBLOCKDATA
{	
	int blocksize;
	int intraACflag;
	Block8i *intraQuanblck;
	Block8i *intraInverseQuanblck;
	Block8d *intraInverseDCTblck;
	int* intraReorderedblck;

	MotionVector Reconstructedmv;
	int interACflag;
	Block8i *interQuanblck;
	Block8i *interInverseQuanblck;
	Block8d *interInverseDCTblck;
	int* interReorderedblck;
}DCBlockData;


typedef struct __DFRAMEDATA
{
	DBlockData  *dblocks;
	DCBlockData *dcbblocks;
	DCBlockData *dcrblocks;

	int blocksize1;
	int blocksize2;
	int nblocks16;
	int nblocks8;
	int nblckloop;
	int splitWidth;
	int splitHeight;
	int numOfFrame;
			
	int CbCrWidth;
	int CbCrHeight;
	int CbCrSplitWidth;
	int CbCrSplitHeight;

	unsigned char* decodedY;
	unsigned char* decodedCb;
	unsigned char* decodedCr;
}DFrameData;

class IcspCodec
{
public:
	DFrameData* dframes;
	int nframes;
	int height;
	int width;
	int QstepDC;
	int QstepAC;
	int DPCMmode;
	int mvPredmode;
	int intraPeriod;
	int intraPredictionMode;

public:
	void init(char* binfname, int nframes);	
	void decoding(char* imgfname);
	~IcspCodec();
};



int loadbinCIF(IcspCodec &icC, char* binfname, FILE* fp);
int readHeader(IcspCodec &icC, header &hd, FILE* fp);
int readBlockData(IcspCodec& icC, FILE* fp);
int DCientropy(unsigned char* bitchar, IntrabodyY& intrabdY);
int ACientropy(unsigned char* bitchar, IntrabodyY& intrabdY);
int DCientropy(unsigned char* bitchar, IntrabodyCbCr& intrabdCbCr);
int ACientropy(unsigned char* bitchar, IntrabodyCbCr& intrabdCbCr);


int DCientropy(unsigned char* bitchar, InterbodyY& interbdY);
int DCientropy(unsigned char* bitchar, InterbodyCbCr& interbdCbCr);
int ACientropy(unsigned char* bitchar, InterbodyY& interbdY);
int ACientropy(unsigned char* bitchar, InterbodyCbCr& interbdCbCr);

void intraDblckDataInit(DBlockData& dbd, IntrabodyY bdy, int nblck, int blocksize1, int blocksize2);
void intraDCblckDataInit(DCBlockData& dcbd, IntrabodyCbCr intrabdcbcr, int blocksize);

void interDblckDataInit(DBlockData& dbd, InterbodyY bdy, int nblck, int blocksize1, int blocksize2);
void interDCblckDataInit(DCBlockData& dcbd, InterbodyCbCr interbdcbcr, int blocksize);


/* intra prediction function */
void allintraPredictionDecode(DFrameData *dframes, int nframes, int QstepDC, int QstepAC);
void intraPredictionDecode(DFrameData& dfrm, int QstepDC, int QstepAC);
void IDPCM_pix_0(unsigned char upper[][8], double current[][8], unsigned char restored_temp[][8], int blocksize);
void IDPCM_pix_1(unsigned char left[][8], double current[][8], unsigned char restored_temp[][8], int blocksize);
void IDPCM_pix_2(unsigned char left[][8], unsigned char upper[][8], double current[][8], unsigned char restored_temp[][8], int blocksize);
void IDPCM_pix_block(DFrameData &frm, int numOfblck16, int numOfblck8, int blocksize, int splitWidth);
void intraImgReconstruct(DFrameData &frm);
void intraCbCr(DFrameData& dfrm, DCBlockData &dcbbd, DCBlockData &dcrbd, int blocksize, int numOfblck8, int QstepDC, int QstepAC);


/* inter prediction function */
void interPredictionDecode(DFrameData& dcntFrm, DFrameData& dprevFrm, int QstepDC, int QstepAC);
void get16block(unsigned char* img, unsigned char *dst[16], int y0, int x0, int width, int blocksize);
void getPaddingImage(unsigned char* src, unsigned char* dst, int padWidth, int padlen, int width, int height);
void interCbCr(DFrameData& dcntFrm, DFrameData& dprevFrm, int QstepDC, int QstepAC);
int MVientropy(unsigned char* bitchar, MotionVector& dmv);
void ImvPrediction(DFrameData& dcntFrm, int nblck);
void interYReconstruct(DFrameData& dcntFrm, DFrameData& dprevFrm);
void interCbCrReconstruct(DFrameData& dcntFrm, DFrameData& dprevFrm);


/* common function */
void reorderingblck(DBlockData& dbd, int numOfblck8, int predmode);
void izigzagScanning(int* src, Block8i& dst, int blocksize);
void izzf(int* src, Block8i& dst, int nloop, int begindx, int blocksize);
void IQuantization_block(DBlockData &dbd, int numOfblck8, int blocksize, int QstepDC, int QstepAC, int type);
void IDPCM_DC_block(DFrameData &frm, int numOfblck16, int numOfblck8, int blocksize, int splitWidth, int predmode);
void IDCT_block(DBlockData &bd, int numOfblck8, int blocksize, int predmode);
void mergeBlock(DBlockData &bd, int blocksize, int predmode);
void CIQuantization_block(DCBlockData& dcbd, int blocksize, int QstepDC, int QstepAC, int predmode);
void CIDPCM_DC_block(DFrameData& dfrm, DCBlockData& dcbd, int numOfblck8, int CbCrtpye, int predmode);
void CIDCT_block(DCBlockData &dcbd, int blocksize, int predmode);
void Creorderingblck(DCBlockData& dcbd, int predmode);

void checkResultFrames(DFrameData* frm, int width, int height, int nframe, int predtype, int chtype);

inline void IcspCodec::init(char* binfname, int nframes)
{
	
	char temp[256];
	sprintf(temp, "output\\%s", binfname);

	FILE* fp = fopen(temp, "rb");
	if(fp==NULL)
	{
		cout << "fail to load " << temp << endl;
		exit(-1);
	}

	if(loadbinCIF(*this, binfname, fp)!=0)
	{
		cout << "error in loadbinCIF" << endl;
		exit(-1);
	}

	

	this->nframes = nframes;
	dframes = (DFrameData *)malloc(sizeof(DFrameData)*nframes);
	if(dframes==NULL)
	{
		cout << "fail to allocate dframes memory" << endl;
		exit(-1);
	}


	for(int i=0; i<nframes; i++)
	{
		dframes[i].blocksize1  = 16;
		dframes[i].blocksize2  = 8;
		dframes[i].nblocks16   = (width*height)/(dframes[i].blocksize1*dframes[i].blocksize1);
		dframes[i].nblocks8    = (width*height)/(dframes[i].blocksize2*dframes[i].blocksize2);
		dframes[i].nblckloop   = (dframes[i].blocksize1*dframes[i].blocksize1)/(dframes[i].blocksize2*dframes[i].blocksize2);
		dframes[i].splitWidth  = width / dframes[i].blocksize1;
		dframes[i].splitHeight = height / dframes[i].blocksize1;

		dframes[i].CbCrHeight  = height / 2;
		dframes[i].CbCrWidth   = width  / 2;
		dframes[i].CbCrSplitWidth = dframes[i].splitWidth;
		dframes[i].CbCrSplitHeight = dframes[i].splitHeight;
	}
		
	if(readBlockData(*this, fp)!=0)
	{
		cout << "error in readBlockData" << endl;
		exit(-1);
	}
	fclose(fp);
}
inline void IcspCodec::decoding(char *imgfname)
{
	clock_t t = clock();
	if( intraPeriod==1 )
	{
		allintraPredictionDecode(dframes, nframes, QstepDC, QstepAC);
		checkResultFrames(dframes,  width, height, nframes, INTRA, SAVE_YUV);
	}
	else
	{
		for(int n=0; n<nframes; n++)
		{
			if(n%intraPeriod==0)
			{
				intraPredictionDecode(dframes[n], QstepDC, QstepAC);
			}
			else
			{
				interPredictionDecode(dframes[n], dframes[n-1], QstepDC, QstepAC);
			}
			
		}
		checkResultFrames(dframes,  width, height, nframes, INTER, SAVE_YUV);
	}
	
	double detime = (double)(clock()-t)/CLOCKS_PER_SEC;
	int width = dframes->blocksize1 * dframes->splitWidth;
	int height = dframes->blocksize1 * dframes->splitHeight;
	double psnr=0;
	double MSE=0;
	double val=0;
	unsigned char* origimg = (unsigned char*)malloc(sizeof(unsigned char)*width*height);
	char temp[256];
	sprintf(temp, "data\\%s", imgfname);
	FILE* imgfp = fopen(temp, "rb");
	if(imgfp == NULL)
	{
		cout << " error in imgfp" << endl;
		exit(-1);
	}

	for(int nfrm=0; nfrm<nframes; nfrm++)
	{
		fread(origimg, sizeof(unsigned char)*width*height, 1, imgfp);
		fseek(imgfp, (width/2)*(height/2)*2, SEEK_CUR);
		MSE = 0;
		for(int y=0; y<height; y++)
		{
			for(int x=0; x<width; x++)
			{
				val = ((double)origimg[y*width+x] - (double)dframes[nfrm].decodedY[y*width+x]) * ((double)origimg[y*width+x] - (double)dframes[nfrm].decodedY[y*width+x]);
				MSE += val;
			}
		}
		
		MSE /= (width*height);
		psnr += (20. * log10(255./sqrt(MSE)));
	}
	psnr /= (double)nframes;
	char outputtxt[256];
	sprintf(outputtxt, "decoding time: %.4lf(s) PSNR: %.4lf QPDC: %d  QPAC: %d Period: %d\n", detime, psnr, QstepDC, QstepAC, intraPeriod);
	FILE* fp = fopen("experimental_Result_Decoding.txt", "at");
	fprintf(fp, "%s", outputtxt);

	fclose(fp);
	fclose(imgfp);
}

inline IcspCodec::~IcspCodec()
{
	for(int i=0; i<nframes; i++)
	{
		free(dframes[i].dblocks);
		free(dframes[i].dcbblocks);
		free(dframes[i].dcrblocks);
	}
}