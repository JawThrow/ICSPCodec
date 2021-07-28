#include "ICSP_Codec_Encoder.h"

extern char filename[256];
int main(int argc, char *argv[])
{	
	cmd_options_t options;	
	set_command_options(argc, argv, &options);

	// split sequence name for saving bitstream output
	char *yuv_fname = options.yuv_fname;
	int idx = 0;
	char* p = yuv_fname;
	while (*p++ != '_')
		idx++;
	
	memcpy(filename, yuv_fname, idx);
	filename[idx] = 0;

	IcspCodec icspCodec;
	icspCodec.init(options.total_frames, options.yuv_fname, 352, 288, options.QP_DC, options.QP_AC);
	icspCodec.encoding(&options);
	
	return 0;
}
