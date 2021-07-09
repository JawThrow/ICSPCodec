#include "ICSP_Codec_Encoder.h"

extern char filename[256];
// argv: [0]:ICSPCodec [1]sequence_fname [2]nframes [3]QPDC [4]QPAC [5]IntraPeroid
int main(int argc, char *argv[])
{	
	char *imgfname = argv[1]; 
	int nframes = atoi(argv[2]);	
	int QPDC = atoi(argv[3]);
	int QPAC = atoi(argv[4]);
	int intraPeriod = atoi(argv[5]);

	// split sequence name for saving compressed output
	int idx = 0;
	char* p = imgfname;
	while (*p++ != '_')
		idx++;
	
	memcpy(filename, imgfname, idx);
	filename[idx] = 0;

	IcspCodec icspCodec;
	icspCodec.init(nframes, imgfname, 352, 288, QPDC, QPAC);
	icspCodec.encoding(intraPeriod);
	
	return 0;
}
