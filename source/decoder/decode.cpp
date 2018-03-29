#include "ICSP_Codec_Decoder.h"


int main(int argc, char *argv[])
{
	int nframes = atoi(argv[1]);
	char* binfname = argv[2]; 
	int QPDC = atoi(argv[3]);
	int QPAC = atoi(argv[4]);
	int intraPeriod = atoi(argv[5]);
	char imgfname[256];
	sprintf(imgfname, argv[6]);



	IcspCodec icspCodec;
	icspCodec.init(binfname, nframes);
	icspCodec.decoding(imgfname);
	return 0;
}

//