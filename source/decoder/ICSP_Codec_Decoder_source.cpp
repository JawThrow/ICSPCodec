#include "ICSP_Codec_Decoder.h"

int loadbinCIF(IcspCodec &icC, char* binfname, FILE* fp)
{
	header hd;	
	if(readHeader(icC, hd, fp)!=0)
	{
		cout << "error in readHeader" << endl;
		return -1;
	}
	
	return 0;
}
int readHeader(IcspCodec& icC, header &hd, FILE* fp)
{		
	fread(&hd, sizeof(header), 1, fp);

	if(hd.intro[0]!=0 && hd.intro[1]!=73 && hd.intro[2]!=67 && hd.intro[3]!=83 && hd.intro[4]!=80)
	{
		cout << "this bin file is not icspCodec file" << endl;
		return -1;
	}

	icC.height = hd.height;
	icC.width  = hd.width;
	icC.QstepDC = hd.QP_DC;
	icC.QstepAC = hd.QP_AC;
	icC.DPCMmode = hd.DPCMmode;
	icC.intraPeriod = (hd.outro & 0x1F80) >> 7;
	icC.intraPredictionMode = (hd.outro & 0x0040) >> 6;


	//cout << "intraperiod: " << icC.intraPeriod << endl;


	return 0;
}
int readBlockData(IcspCodec& icC, FILE* fp)
{
	int byteslength=0;
	int totalbits=0;
	while(fgetc(fp)!=EOF)
	{
		byteslength++;
	}
	totalbits = byteslength*8;

	//cout << "전체 bits: " << totalbits << " 전체 바이트수 : " << byteslength << endl;
	fseek(fp, sizeof(header), SEEK_SET);
	unsigned char* bodystream = (unsigned char*)calloc( byteslength, sizeof(unsigned char) ); // 압축된 모든 프레임에 대한 body
	if(bodystream==NULL)
	{
		cout << "fail to allocate bodystream memory" << endl;
		return -1;
	}

	for(int i=0; i<byteslength; i++)
		bodystream[i] = fgetc(fp);

	unsigned char* bitchar = (unsigned char*)calloc( totalbits, sizeof(unsigned char) );
	if(bitchar==NULL)
	{
		cout << "fail to allocate bodystream memory" << endl;
		return -1;
	}

	int idx=0;
	for(int i=0; i<byteslength; i++)
	{
		for(int n=7; n>=0; n--)
		{
			bitchar[idx++] = (bodystream[i]>>n)&1;
		}
	}

	int maxframes = MAXFRAMES;
	int nframes = icC.nframes;
	int blocksize1 = icC.dframes->blocksize1;
	int blocksize2 = icC.dframes->blocksize2;

	int totalblck = icC.dframes->nblocks16;
	int nblckloop = icC.dframes->nblckloop;

	
	int cntidx=0;
	int cntblck=0;
	int frmbits=0;
	int prevbits=0;
	IntrabodyY	  intrabdY;
	IntrabodyCbCr intrabdCb; 
	IntrabodyCbCr intrabdCr;

	InterbodyY	  interbdY;
	InterbodyCbCr interbdCb;
	InterbodyCbCr interbdCr;

	// all intra prediction
	if(icC.intraPeriod==1)
	{
		for(int n=0; n<nframes; n++)
		{
			icC.dframes[n].dblocks   = (DBlockData*)malloc(sizeof(DBlockData)*totalblck);	// 다쓰고 free
			icC.dframes[n].dcbblocks = (DCBlockData*)malloc(sizeof(DCBlockData)*totalblck);
			icC.dframes[n].dcrblocks = (DCBlockData*)malloc(sizeof(DCBlockData)*totalblck);
			prevbits = cntidx; // bits 수 확인하려고

			// Y channel
			for(int nblck16=0; nblck16<totalblck; nblck16++)
			{
				DBlockData& dbd = icC.dframes[n].dblocks[nblck16];

				for(int nblck=0; nblck<nblckloop; nblck++)
				{
					intrabdY.mpmflag = bitchar[cntidx++];				// mpmflag 저장	
					intrabdY.intraPredictionMode = bitchar[cntidx++];   // intra prediction mode 저장

					// DC값 decoding
					int DCbits=0;
					DCbits = DCientropy(&bitchar[cntidx], intrabdY);
					cntidx += DCbits;

					intrabdY.ACflag = bitchar[cntidx++];  // ACflag 저장
				
					// AC값 decoding
					if(intrabdY.ACflag == 1)
					{
						cntidx += 63;
						for(int i=0; i<63; i++)
							intrabdY.ACvals[i] = 0;
					}
					else if(intrabdY.ACflag == 0)
					{
						int ACbits=0;
						ACbits = ACientropy(&bitchar[cntidx], intrabdY);
						cntidx += ACbits;
					}				
					intraDblckDataInit(dbd, intrabdY, nblck, blocksize1, blocksize2);
				}

				// Cb Cr channels
				{
					DCBlockData& dcbbd = icC.dframes[n].dcbblocks[nblck16];
					int DCbits=0;
					DCbits = DCientropy(&bitchar[cntidx], intrabdCb);
					cntidx += DCbits;

					intrabdCb.ACflag = bitchar[cntidx++];  // ACflag 저장

					// AC값 decoding
					if(intrabdCb.ACflag == 1)
					{
						cntidx += 63;
						for(int i=0; i<63; i++)
							intrabdCb.ACvals[i] = 0;
					}
					else if(intrabdCb.ACflag == 0)
					{
						int ACbits=0;
						ACbits = ACientropy(&bitchar[cntidx], intrabdCb);
						cntidx += ACbits;
					}
					intraDCblckDataInit(dcbbd, intrabdCb, blocksize2);
				}

				{
					DCBlockData& dcrbd = icC.dframes[n].dcrblocks[nblck16];
					int DCbits=0;
					DCbits = DCientropy(&bitchar[cntidx], intrabdCr);
					cntidx += DCbits;

					intrabdCr.ACflag = bitchar[cntidx++];  // ACflag 저장

					// AC값 decoding
					if(intrabdCr.ACflag == 1)
					{
						cntidx += 63;
						for(int i=0; i<63; i++)
							intrabdCr.ACvals[i] = 0;
					}
					else if(intrabdCr.ACflag == 0)
					{
						int ACbits=0;
						ACbits = ACientropy(&bitchar[cntidx], intrabdCr);
						cntidx += ACbits;
					}
					intraDCblckDataInit(dcrbd, intrabdCr, blocksize2);
				}
			}		
		}
	}
	else	// not all intra prediction mode
	{
		for(int n=0; n<nframes; n++)
		{
			icC.dframes[n].dblocks   = (DBlockData*)malloc(sizeof(DBlockData)*totalblck);	// 다쓰고 free
			icC.dframes[n].dcbblocks = (DCBlockData*)malloc(sizeof(DCBlockData)*totalblck);
			icC.dframes[n].dcrblocks = (DCBlockData*)malloc(sizeof(DCBlockData)*totalblck);
			prevbits = cntidx; // bits 수 확인하려고

			// intra prediction 
			if(n%icC.intraPeriod==0)
			{
				// Y channel				
				//cout << n << "번째 intra frame bits: " << cntidx << endl;
				for(int nblck16=0; nblck16<totalblck; nblck16++)
				{
					DBlockData& dbd = icC.dframes[n].dblocks[nblck16];

					//cout << nblck16 << " blocks" << endl;
					for(int nblck=0; nblck<nblckloop; nblck++)
					{
						intrabdY.mpmflag = bitchar[cntidx++];				// mpmflag 저장	
						intrabdY.intraPredictionMode = bitchar[cntidx++];   // intra prediction mode 저장

						// DC값 decoding
						int DCbits=0;
						DCbits = DCientropy(&bitchar[cntidx], intrabdY);
						cntidx += DCbits;

						//cout << "Y dc bits: " << cntidx << endl;

						intrabdY.ACflag = bitchar[cntidx++];  // ACflag 저장

						// AC값 decoding
						if(intrabdY.ACflag == 1)
						{
							cntidx += 63;
							for(int i=0; i<63; i++)
								intrabdY.ACvals[i] = 0;
						}
						else if(intrabdY.ACflag == 0)
						{
							int ACbits=0;
							ACbits = ACientropy(&bitchar[cntidx], intrabdY);
							cntidx += ACbits;
						}				
						intraDblckDataInit(dbd, intrabdY, nblck, blocksize1, blocksize2);
						//cout << "Y ac bits: " << cntidx << endl;
					}

					// Cb Cr channels
					{
						DCBlockData& dcbbd = icC.dframes[n].dcbblocks[nblck16];
						int DCbits=0;
						DCbits = DCientropy(&bitchar[cntidx], intrabdCb);
						cntidx += DCbits;

						//cout << "cb dc bits: " << cntidx << endl;

						intrabdCb.ACflag = bitchar[cntidx++];  // ACflag 저장

						// AC값 decoding
						if(intrabdCb.ACflag == 1)
						{
							cntidx += 63;
							for(int i=0; i<63; i++)
								intrabdCb.ACvals[i] = 0;
						}
						else if(intrabdCb.ACflag == 0)
						{
							int ACbits=0;
							ACbits = ACientropy(&bitchar[cntidx], intrabdCb);
							cntidx += ACbits;
						}
						intraDCblckDataInit(dcbbd, intrabdCb, blocksize2);
						//cout << "cb ac bits: " << cntidx << endl;
					}

					{
						DCBlockData& dcrbd = icC.dframes[n].dcrblocks[nblck16];
						int DCbits=0;
						DCbits = DCientropy(&bitchar[cntidx], intrabdCr);
						cntidx += DCbits;

						//cout << "cr dc bits: " << cntidx << endl;

						intrabdCr.ACflag = bitchar[cntidx++];  // ACflag 저장

						// AC값 decoding
						if(intrabdCr.ACflag == 1)
						{
							cntidx += 63;
							for(int i=0; i<63; i++)
								intrabdCr.ACvals[i] = 0;
						}
						else if(intrabdCr.ACflag == 0)
						{
							int ACbits=0;
							ACbits = ACientropy(&bitchar[cntidx], intrabdCr);
							cntidx += ACbits;
						}
						intraDCblckDataInit(dcrbd, intrabdCr, blocksize2);

						//cout << "cr ac bits: " << cntidx << endl;
						//system("pause");
					}					
				}
				//cout << n << "번째 intra frame 읽은 후 bits: " << cntidx << endl;
				//system("pause");				
			}
			else  // inter prediction
			{
				// Y channel
				for(int nblck16=0; nblck16<totalblck; nblck16++)
				{
					DBlockData& dbd = icC.dframes[n].dblocks[nblck16];
					dbd.MVmodeflag =  bitchar[cntidx++]; // MVmodeflag 저장
					MotionVector dmv;
					dmv.x=dmv.y=0;

					int MVbits=0;
					MVbits = MVientropy(&bitchar[cntidx], dmv);					
					cntidx += MVbits;

					dbd.Reconstructedmv.x = dmv.x;
					dbd.Reconstructedmv.y = dmv.y;

					for(int nblck=0; nblck<nblckloop; nblck++)
					{
						// DC값 decoding
						int DCbits=0;
						DCbits = DCientropy(&bitchar[cntidx], interbdY);
						cntidx += DCbits;

						interbdY.ACflag = bitchar[cntidx++];  // ACflag 저장

						// AC값 decoding
						if(interbdY.ACflag == 1)
						{
							cntidx += 63;
							for(int i=0; i<63; i++)
								interbdY.ACvals[i] = 0;
						}
						else if(interbdY.ACflag == 0)
						{
							int ACbits=0;
							ACbits = ACientropy(&bitchar[cntidx], interbdY);
							cntidx += ACbits;
						}				
						interDblckDataInit(dbd, interbdY, nblck, blocksize1, blocksize2);
					}
					//cout << "Yframe bits: " << cntidx << endl;

					// Cb Cr channels
					{DCBlockData& dcbbd = icC.dframes[n].dcbblocks[nblck16];
					int DCbits=0;
					DCbits = DCientropy(&bitchar[cntidx], interbdCb);
					cntidx += DCbits;

					interbdCb.ACflag = bitchar[cntidx++];  // ACflag 저장

					// AC값 decoding
					if(interbdCb.ACflag == 1)
					{
						cntidx += 63;
						for(int i=0; i<63; i++)
							interbdCb.ACvals[i] = 0;
					}
					else if(interbdCb.ACflag == 0)
					{
						int ACbits=0;
						ACbits = ACientropy(&bitchar[cntidx], interbdCb);
						cntidx += ACbits;
					}
					interDCblckDataInit(dcbbd, interbdCb, blocksize2);}
					//cout << "cbframe bits: " << cntidx << endl;

					{DCBlockData& dcrbd = icC.dframes[n].dcrblocks[nblck16];
					int DCbits=0;
					DCbits = DCientropy(&bitchar[cntidx], interbdCr);
					cntidx += DCbits;
					//cout << "cr의 DC bits: " << cntidx << endl;

					interbdCr.ACflag = bitchar[cntidx++];  // ACflag 저장

					// AC값 decoding
					if(interbdCr.ACflag == 1)
					{
						cntidx += 63;
						for(int i=0; i<63; i++)
							interbdCr.ACvals[i] = 0;
						//cout << "acflag: 1" << endl;
					}
					else if(interbdCr.ACflag == 0)
					{
						int ACbits=0;
						ACbits = ACientropy(&bitchar[cntidx], interbdCr);
						cntidx += ACbits;
						//cout << "acflag: 0" << endl;
					}
					interDCblckDataInit(dcrbd, interbdCr, blocksize2);}
					/*cout << "cr의 AC bits: " << cntidx << endl;
					cout << "crframe bits: " << cntidx << endl;
					system("pause");*/
					
				}
			}
			//cout << n << "번째 프레임 bits: " << cntidx << endl;
		}
	}
	free(bitchar);
	free(bodystream);
	return 0;
}

/* intra */
int DCientropy(unsigned char* bitchar, IntrabodyY& intrabdY)
{
	int cntidx   = 0;
	int val		 = 0;
	int signidx  = 0;
	int rangelen = 0;
	int rangeidx = 0;
	int codeword = 0;
	int temp	 = 0;
	int len      = 0;
	

	if(bitchar[0]==0 && bitchar[1]==0) // cate 0: 0
	{
		len = 2;
		val = 0;
	}
	else if(bitchar[0]==0 && bitchar[1]==1 && bitchar[2]==0) // cate 1: 1
	{
		len = 4;
		signidx=3;
		val = (bitchar[signidx]==1)? 1:-1;
	}
	else if(bitchar[0]==0 && bitchar[1]==1 && bitchar[2]==1) // cate 2: 2~3
	{
		len=5; codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[0]==1 && bitchar[1]==0 && bitchar[2]==0) // cate 3: 4~7
	{
		len=6;codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 4+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[0]==1 && bitchar[1]==0 && bitchar[2]==1) // cate 4: 8~15
	{
		len=7;codeword=3;
		signidx=3;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 8+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==0) // cate 5: 16~31
	{
		len=8;codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 16+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 6: 32~63
			bitchar[3]==0) 
	{
		len=10;codeword=4;
		signidx=codeword;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 32+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 7: 64~127
			bitchar[3]==1 && bitchar[4]==0) 
	{
		len=12;codeword=5;
		signidx=5;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 64+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 8: 128~255
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==0) 
	{
		len=14;codeword=6;
		signidx=6;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 128+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 9: 256~511
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==0)
	{
		len=16;codeword=7;
		signidx=7;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 256+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 10: 512~1023
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 && 
			bitchar[6]==1 && bitchar[7]==0) 
	{
		len=18;codeword=8;
		signidx=8;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 512+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 11: 1024~2047
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==1 && bitchar[7]==1 && bitchar[8]==0) 
	{
		len=20;codeword=9;
		signidx=9;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 1024+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 12: 2048
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==1 && bitchar[7]==1 && bitchar[8]==1 && bitchar[9]==0) 
	{
		len=22;codeword=10;
		signidx=10;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2048+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	
	/*cout << "DC ientropy" << endl;
	for(int i=0; i<len; i++)
	{
		cout << (int)bitchar[i] << " ";
	}
	cout << endl;*/

	intrabdY.DCval = val;
	cntidx = len;
	return cntidx;
}
int DCientropy(unsigned char* bitchar, IntrabodyCbCr& intrabdCbCr)
{
	int cntidx   = 0;
	int val		 = 0;
	int signidx  = 0;
	int rangelen = 0;
	int rangeidx = 0;
	int codeword = 0;
	int temp	 = 0;
	int len      = 0;
	

	if(bitchar[0]==0 && bitchar[1]==0) // cate 0: 0
	{
		len = 2;
		val = 0;
	}
	else if(bitchar[0]==0 && bitchar[1]==1 && bitchar[2]==0) // cate 1: 1
	{
		len = 4;
		signidx=3;
		val = (bitchar[signidx]==1)? 1:-1;
	}
	else if(bitchar[0]==0 && bitchar[1]==1 && bitchar[2]==1) // cate 2: 2~3
	{
		len=5; codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[0]==1 && bitchar[1]==0 && bitchar[2]==0) // cate 3: 4~7
	{
		len=6;codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 4+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[0]==1 && bitchar[1]==0 && bitchar[2]==1) // cate 4: 8~15
	{
		len=7;codeword=3;
		signidx=3;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 8+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==0) // cate 5: 16~31
	{
		len=8;codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 16+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 6: 32~63
			bitchar[3]==0) 
	{
		len=10;codeword=4;
		signidx=codeword;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 32+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 7: 64~127
			bitchar[3]==1 && bitchar[4]==0) 
	{
		len=12;codeword=5;
		signidx=5;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 64+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 8: 128~255
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==0) 
	{
		len=14;codeword=6;
		signidx=6;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 128+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 9: 256~511
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==0)
	{
		len=16;codeword=7;
		signidx=7;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 256+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 10: 512~1023
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 && 
			bitchar[6]==1 && bitchar[7]==0) 
	{
		len=18;codeword=8;
		signidx=8;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 512+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 11: 1024~2047
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==1 && bitchar[7]==1 && bitchar[8]==0) 
	{
		len=20;codeword=9;
		signidx=9;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 1024+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 12: 2048
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==1 && bitchar[7]==1 && bitchar[8]==1 && bitchar[9]==0) 
	{
		len=22;codeword=10;
		signidx=10;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2048+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}

	/*cout << "DC ientropy" << endl;
	for(int i=0; i<len; i++)
	{
		cout << (int)bitchar[i] << " ";
	}
	cout << endl;*/
	intrabdCbCr.DCval = val;
	cntidx = len;
	return cntidx;
}
int ACientropy(unsigned char* bitchar, IntrabodyY& intrabdY)
{
	int len      = 0;
	int nbits    = 0;
	int nbyte    = 0;
	int last     = 0;
	int signidx  = 0;
	int codeword = 0;
	int temp	 = 0;
	int val		 = 0;
	int rangeidx = 0;
	int rangelen = 0;


	//cout << "AC ientropy" << endl;
	for(int i=0; i<63; i++)
	{
		/////
		int prevlast = last;
		////

		if(bitchar[last]==0 && bitchar[last+1]==0) // cate 0: 0
		{
			len=2;
			val=0;
		}
		else if(bitchar[last]==0 && bitchar[last+1]==1 && bitchar[last+2]==0) // cate 1: 1
		{
			len=4;
			signidx=last+3;
			val = (bitchar[signidx]==1)? 1:-1;
		}
		else if(bitchar[last]==0 && bitchar[last+1]==1 && bitchar[last+2]==1) // cate 2: 2~3
		{
			len=5; codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 2+temp;
			val = (bitchar[signidx]==1)? val:-val;

		}
		else if(bitchar[last]==1 && bitchar[last+1]==0 && bitchar[last+2]==0) // cate 3: 4~7
		{
			len=6;codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 4+temp;
			val = (bitchar[signidx]==1)? val:-val;

		}
		else if(bitchar[last]==1 && bitchar[last+1]==0 && bitchar[last+2]==1) // cate 4: 8~15
		{
			len=7;codeword=3;
			signidx=last+3;
			temp=0; rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 8+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]==1 && bitchar[last+1]==1 && bitchar[last+2]==0) // cate 5: 16~31
		{
			len=8;codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 16+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 6: 32~63
				bitchar[last+3]==0) 
		{
			len=10;codeword=4;
			signidx=last+codeword;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 32+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 7: 64~127
				bitchar[last+3]==1 && bitchar[last+4]==0) 
		{
			len=12;codeword=5;
			signidx=last+5;
			temp=0; rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 64+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 8: 128~255
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==0) 
		{
			len=14;codeword=6;
			signidx=last+6;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 128+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 9: 256~511
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==0)
		{
			len=16;codeword=7;
			signidx=last+7;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 256+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 10: 512~1023
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 && 
				bitchar[last+6]==1 && bitchar[last+7]==0) 
		{
			len=18;codeword=8;
			signidx=last+8;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 512+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 11: 1024~2047
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==0) 
		{
			len=20;codeword=9;
			signidx=last+9;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 1024+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 12: 2048
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==1 && bitchar[last+9]==0) 
		{
			len=22;codeword=10;
			signidx=last+10;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 2048+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		last += len;
		intrabdY.ACvals[i] = val;

		/*for(int nb=prevlast; nb<last; nb++)
		{
			cout << (int)bitchar[nb] << " ";
		}
		cout << endl;*/
	}
	//system("pause");
	
	return last;
}
int ACientropy(unsigned char* bitchar, IntrabodyCbCr& intrabdCbCr)
{
	int len      = 0;
	int nbits    = 0;
	int nbyte    = 0;
	int last     = 0;
	int signidx  = 0;
	int codeword = 0;
	int temp	 = 0;
	int val		 = 0;
	int rangeidx = 0;
	int rangelen = 0;

	//cout << "AC entropy" << endl;
	for(int i=0; i<63; i++)
	{
		int prevlast = last;
		if(bitchar[last]==0 && bitchar[last+1]==0) // cate 0: 0
		{
			len=2;
			val=0;
		}
		else if(bitchar[last]==0 && bitchar[last+1]==1 && bitchar[last+2]==0) // cate 1: 1
		{
			len=4;
			signidx=last+3;
			val = (bitchar[signidx]==1)? 1:-1;
		}
		else if(bitchar[last]==0 && bitchar[last+1]==1 && bitchar[last+2]==1) // cate 2: 2~3
		{
			len=5; codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 2+temp;
			val = (bitchar[signidx]==1)? val:-val;

		}
		else if(bitchar[last]==1 && bitchar[last+1]==0 && bitchar[last+2]==0) // cate 3: 4~7
		{
			len=6;codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 4+temp;
			val = (bitchar[signidx]==1)? val:-val;

		}
		else if(bitchar[last]==1 && bitchar[last+1]==0 && bitchar[last+2]==1) // cate 4: 8~15
		{
			len=7;codeword=3;
			signidx=last+3;
			temp=0; rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 8+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]==1 && bitchar[last+1]==1 && bitchar[last+2]==0) // cate 5: 16~31
		{
			len=8;codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 16+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 6: 32~63
				bitchar[last+3]==0) 
		{
			len=10;codeword=4;
			signidx=last+codeword;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 32+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 7: 64~127
				bitchar[last+3]==1 && bitchar[last+4]==0) 
		{
			len=12;codeword=5;
			signidx=last+5;
			temp=0; rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 64+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 8: 128~255
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==0) 
		{
			len=14;codeword=6;
			signidx=last+6;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 128+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 9: 256~511
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==0)
		{
			len=16;codeword=7;
			signidx=last+7;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 256+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 10: 512~1023
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 && 
				bitchar[last+6]==1 && bitchar[last+7]==0) 
		{
			len=18;codeword=8;
			signidx=last+8;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 512+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 11: 1024~2047
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==0) 
		{
			len=20;codeword=9;
			signidx=last+9;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 1024+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 12: 2048
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==1 && bitchar[last+9]==0) 
		{
			len=22;codeword=10;
			signidx=last+10;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 2048+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		last += len;
		intrabdCbCr.ACvals[i] = val;
		/*for(int nb=prevlast; nb<last; nb++)
		{
			cout << (int)bitchar[nb] << " ";
		}
		cout << endl;*/
	}
	//system("pause");


	return last;
}


/* inter */
int DCientropy(unsigned char* bitchar, InterbodyY& interbdY)
{
	int cntidx   = 0;
	int val		 = 0;
	int signidx  = 0;
	int rangelen = 0;
	int rangeidx = 0;
	int codeword = 0;
	int temp	 = 0;
	int len      = 0;
	

	if(bitchar[0]==0 && bitchar[1]==0) // cate 0: 0
	{
		len = 2;
		val = 0;
	}
	else if(bitchar[0]==0 && bitchar[1]==1 && bitchar[2]==0) // cate 1: 1
	{
		len = 4;
		signidx=3;
		val = (bitchar[signidx]==1)? 1:-1;
	}
	else if(bitchar[0]==0 && bitchar[1]==1 && bitchar[2]==1) // cate 2: 2~3
	{
		len=5; codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[0]==1 && bitchar[1]==0 && bitchar[2]==0) // cate 3: 4~7
	{
		len=6;codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 4+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[0]==1 && bitchar[1]==0 && bitchar[2]==1) // cate 4: 8~15
	{
		len=7;codeword=3;
		signidx=3;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 8+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==0) // cate 5: 16~31
	{
		len=8;codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 16+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 6: 32~63
			bitchar[3]==0) 
	{
		len=10;codeword=4;
		signidx=codeword;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 32+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 7: 64~127
			bitchar[3]==1 && bitchar[4]==0) 
	{
		len=12;codeword=5;
		signidx=5;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 64+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 8: 128~255
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==0) 
	{
		len=14;codeword=6;
		signidx=6;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 128+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 9: 256~511
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==0)
	{
		len=16;codeword=7;
		signidx=7;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 256+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 10: 512~1023
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 && 
			bitchar[6]==1 && bitchar[7]==0) 
	{
		len=18;codeword=8;
		signidx=8;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 512+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 11: 1024~2047
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==1 && bitchar[7]==1 && bitchar[8]==0) 
	{
		len=20;codeword=9;
		signidx=9;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 1024+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 12: 2048
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==1 && bitchar[7]==1 && bitchar[8]==1 && bitchar[9]==0) 
	{
		len=22;codeword=10;
		signidx=10;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2048+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	
	interbdY.DCval = val;
	cntidx = len;
	return cntidx;
}
int DCientropy(unsigned char* bitchar, InterbodyCbCr& interbdCbCr)
{
	int cntidx   = 0;
	int val		 = 0;
	int signidx  = 0;
	int rangelen = 0;
	int rangeidx = 0;
	int codeword = 0;
	int temp	 = 0;
	int len      = 0;
	

	if(bitchar[0]==0 && bitchar[1]==0) // cate 0: 0
	{
		len = 2;
		val = 0;
	}
	else if(bitchar[0]==0 && bitchar[1]==1 && bitchar[2]==0) // cate 1: 1
	{
		len = 4;
		signidx=3;
		val = (bitchar[signidx]==1)? 1:-1;
	}
	else if(bitchar[0]==0 && bitchar[1]==1 && bitchar[2]==1) // cate 2: 2~3
	{
		len=5; codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[0]==1 && bitchar[1]==0 && bitchar[2]==0) // cate 3: 4~7
	{
		len=6;codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 4+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[0]==1 && bitchar[1]==0 && bitchar[2]==1) // cate 4: 8~15
	{
		len=7;codeword=3;
		signidx=3;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 8+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==0) // cate 5: 16~31
	{
		len=8;codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 16+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 6: 32~63
			bitchar[3]==0) 
	{
		len=10;codeword=4;
		signidx=codeword;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 32+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 7: 64~127
			bitchar[3]==1 && bitchar[4]==0) 
	{
		len=12;codeword=5;
		signidx=5;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 64+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 8: 128~255
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==0) 
	{
		len=14;codeword=6;
		signidx=6;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 128+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 9: 256~511
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==0)
	{
		len=16;codeword=7;
		signidx=7;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 256+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 10: 512~1023
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 && 
			bitchar[6]==1 && bitchar[7]==0) 
	{
		len=18;codeword=8;
		signidx=8;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 512+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 11: 1024~2047
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==1 && bitchar[7]==1 && bitchar[8]==0) 
	{
		len=20;codeword=9;
		signidx=9;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 1024+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==1 && // cate 12: 2048
			bitchar[3]==1 && bitchar[4]==1 && bitchar[5]==1 &&
			bitchar[6]==1 && bitchar[7]==1 && bitchar[8]==1 && bitchar[9]==0) 
	{
		len=22;codeword=10;
		signidx=10;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2048+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	
	interbdCbCr.DCval = val;
	cntidx = len;
	return cntidx;
}
int ACientropy(unsigned char* bitchar, InterbodyY& interbdY)
{
	int len      = 0;
	int nbits    = 0;
	int nbyte    = 0;
	int last     = 0;
	int signidx  = 0;
	int codeword = 0;
	int temp	 = 0;
	int val		 = 0;
	int rangeidx = 0;
	int rangelen = 0;

	for(int i=0; i<63; i++)
	{
		if(bitchar[last]==0 && bitchar[last+1]==0) // cate 0: 0
		{
			len=2;
			val=0;
		}
		else if(bitchar[last]==0 && bitchar[last+1]==1 && bitchar[last+2]==0) // cate 1: 1
		{
			len=4;
			signidx=last+3;
			val = (bitchar[signidx]==1)? 1:-1;
		}
		else if(bitchar[last]==0 && bitchar[last+1]==1 && bitchar[last+2]==1) // cate 2: 2~3
		{
			len=5; codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 2+temp;
			val = (bitchar[signidx]==1)? val:-val;

		}
		else if(bitchar[last]==1 && bitchar[last+1]==0 && bitchar[last+2]==0) // cate 3: 4~7
		{
			len=6;codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 4+temp;
			val = (bitchar[signidx]==1)? val:-val;

		}
		else if(bitchar[last]==1 && bitchar[last+1]==0 && bitchar[last+2]==1) // cate 4: 8~15
		{
			len=7;codeword=3;
			signidx=last+3;
			temp=0; rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 8+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]==1 && bitchar[last+1]==1 && bitchar[last+2]==0) // cate 5: 16~31
		{
			len=8;codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 16+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 6: 32~63
				bitchar[last+3]==0) 
		{
			len=10;codeword=4;
			signidx=last+codeword;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 32+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 7: 64~127
				bitchar[last+3]==1 && bitchar[last+4]==0) 
		{
			len=12;codeword=5;
			signidx=last+5;
			temp=0; rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 64+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 8: 128~255
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==0) 
		{
			len=14;codeword=6;
			signidx=last+6;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 128+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 9: 256~511
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==0)
		{
			len=16;codeword=7;
			signidx=last+7;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 256+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 10: 512~1023
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 && 
				bitchar[last+6]==1 && bitchar[last+7]==0) 
		{
			len=18;codeword=8;
			signidx=last+8;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 512+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 11: 1024~2047
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==0) 
		{
			len=20;codeword=9;
			signidx=last+9;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 1024+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 12: 2048
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==1 && bitchar[last+9]==0) 
		{
			len=22;codeword=10;
			signidx=last+10;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 2048+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		last += len;
		interbdY.ACvals[i] = val;
	}



	return last;
}
int ACientropy(unsigned char* bitchar, InterbodyCbCr& interbdCbCr)
{
	int len      = 0;
	int nbits    = 0;
	int nbyte    = 0;
	int last     = 0;
	int signidx  = 0;
	int codeword = 0;
	int temp	 = 0;
	int val		 = 0;
	int rangeidx = 0;
	int rangelen = 0;

	for(int i=0; i<63; i++)
	{
		if(bitchar[last]==0 && bitchar[last+1]==0) // cate 0: 0
		{
			len=2;
			val=0;
		}
		else if(bitchar[last]==0 && bitchar[last+1]==1 && bitchar[last+2]==0) // cate 1: 1
		{
			len=4;
			signidx=last+3;
			val = (bitchar[signidx]==1)? 1:-1;
		}
		else if(bitchar[last]==0 && bitchar[last+1]==1 && bitchar[last+2]==1) // cate 2: 2~3
		{
			len=5; codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 2+temp;
			val = (bitchar[signidx]==1)? val:-val;

		}
		else if(bitchar[last]==1 && bitchar[last+1]==0 && bitchar[last+2]==0) // cate 3: 4~7
		{
			len=6;codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 4+temp;
			val = (bitchar[signidx]==1)? val:-val;

		}
		else if(bitchar[last]==1 && bitchar[last+1]==0 && bitchar[last+2]==1) // cate 4: 8~15
		{
			len=7;codeword=3;
			signidx=last+3;
			temp=0; rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 8+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]==1 && bitchar[last+1]==1 && bitchar[last+2]==0) // cate 5: 16~31
		{
			len=8;codeword=3;
			signidx=last+3;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 16+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 6: 32~63
				bitchar[last+3]==0) 
		{
			len=10;codeword=4;
			signidx=last+codeword;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 32+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 7: 64~127
				bitchar[last+3]==1 && bitchar[last+4]==0) 
		{
			len=12;codeword=5;
			signidx=last+5;
			temp=0; rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 64+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 8: 128~255
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==0) 
		{
			len=14;codeword=6;
			signidx=last+6;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 128+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 9: 256~511
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==0)
		{
			len=16;codeword=7;
			signidx=last+7;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 256+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 10: 512~1023
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 && 
				bitchar[last+6]==1 && bitchar[last+7]==0) 
		{
			len=18;codeword=8;
			signidx=last+8;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 512+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 11: 1024~2047
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==0) 
		{
			len=20;codeword=9;
			signidx=last+9;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 1024+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 12: 2048
				bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
				bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==1 && bitchar[last+9]==0) 
		{
			len=22;codeword=10;
			signidx=last+10;
			temp=0;rangeidx=signidx+1;
			rangelen=len-codeword-1;
			for(int n=0; n<rangelen; n++)
			{
				temp |= bitchar[rangeidx+n];
				if(n!=rangelen-1) temp <<= 1;				
			}
			val = 2048+temp;
			val = (bitchar[signidx]==1)? val:-val;
		}
		last += len;
		interbdCbCr.ACvals[i] = val;
	}



	return last;
}

void intraDblckDataInit(DBlockData& dbd, IntrabodyY bdy, int nblck, int blocksize1, int blocksize2)
{
	dbd.blocksize1 = blocksize1;
	dbd.blocksize2 = blocksize2;
	dbd.MPMFlag[nblck] = bdy.mpmflag;
	dbd.intraPredMode[nblck] = bdy.intraPredictionMode;
		
	int *reorderedblck = (int*)malloc(sizeof(int)*blocksize2*blocksize2);	// 다쓰면 free
	reorderedblck[0] = bdy.DCval;
	for(int i=0; i<63; i++)
	{
		reorderedblck[i+1] = bdy.ACvals[i];
	}

	dbd.intraReorderedblck[nblck] = reorderedblck;
}
void intraDCblckDataInit(DCBlockData& dcbd, IntrabodyCbCr intrabdcbcr, int blocksize)
{
	dcbd.blocksize = blocksize;
		
	int *reorderedblck = (int*)malloc(sizeof(int)*blocksize*blocksize);	// 다쓰면 free
	reorderedblck[0] = intrabdcbcr.DCval;
	for(int i=0; i<63; i++)
	{
		reorderedblck[i+1] = intrabdcbcr.ACvals[i];
	}

	dcbd.intraReorderedblck = reorderedblck;
}
void interDblckDataInit(DBlockData& dbd, InterbodyY bdy, int nblck, int blocksize1, int blocksize2)
{
	dbd.blocksize1 = blocksize1;
	dbd.blocksize2 = blocksize2;
		
	int *reorderedblck = (int*)malloc(sizeof(int)*blocksize2*blocksize2);	// 다쓰면 free
	reorderedblck[0] = bdy.DCval;
	for(int i=0; i<63; i++)
	{
		reorderedblck[i+1] = bdy.ACvals[i];
	}
	
	dbd.interReorderedblck[nblck] = reorderedblck;
}
void interDCblckDataInit(DCBlockData& dcbd, InterbodyCbCr interbdcbcr, int blocksize)
{
	dcbd.blocksize = blocksize;
		
	int *reorderedblck = (int*)malloc(sizeof(int)*blocksize*blocksize);	// 다쓰면 free
	reorderedblck[0] = interbdcbcr.DCval;
	for(int i=0; i<63; i++)
	{
		reorderedblck[i+1] = interbdcbcr.ACvals[i];
	}

	dcbd.interReorderedblck = reorderedblck;
}

void allintraPredictionDecode(DFrameData *dframes, int nframes, int QstepDC, int QstepAC)
{
	int totalblck = dframes->nblocks16;
	int nblckloop = dframes->nblckloop;
	int blocksize1 = dframes->blocksize1;
	int blocksize2 = dframes->blocksize2;
	int splitWidth = dframes->splitWidth;
	int splitHeight = dframes->splitHeight;

	for(int numOfFrms=0; numOfFrms<nframes; numOfFrms++)
	{
		DFrameData& dfrm = dframes[numOfFrms];
		for(int numOfblck16=0; numOfblck16<totalblck; numOfblck16++)
		{
			DBlockData& dbd = dfrm.dblocks[numOfblck16];
			DCBlockData& dcbbd = dfrm.dcbblocks[numOfblck16];
			DCBlockData& dcrbd = dfrm.dcrblocks[numOfblck16];
			
			/* 할당구간 */
			dbd.intraQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblckloop);
			for(int i=0; i<nblckloop; i++)
				dbd.intraQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

			dbd.intraInverseQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblckloop);
			for(int i=0; i<nblckloop; i++)
				dbd.intraInverseQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

			dbd.intraInverseDCTblck = (Block8d**)malloc(sizeof(Block8d*)*nblckloop);
			for(int i=0; i<nblckloop; i++)
				dbd.intraInverseDCTblck[i] = (Block8d*)malloc(sizeof(Block8d));

			dbd.intraRestructedblck8 = (Block8u**)malloc(sizeof(Block8u*)*nblckloop);
			for(int i=0; i<nblckloop; i++)
				dbd.intraRestructedblck8[i] = (Block8u*)malloc(sizeof(Block8u));

			dcbbd.intraInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
			dcrbd.intraInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
			dcbbd.intraInverseDCTblck  = (Block8d*)malloc(sizeof(Block8d));
			dcrbd.intraInverseDCTblck  = (Block8d*)malloc(sizeof(Block8d));

			/* 할당구간끝 */

			for(int numOfblck8=0; numOfblck8<nblckloop; numOfblck8++)
			{
				reorderingblck(dbd, numOfblck8, INTRA);
				IQuantization_block(dbd, numOfblck8, blocksize2, QstepDC, QstepAC, INTRA);
				IDPCM_DC_block(dfrm, numOfblck16, numOfblck8, blocksize2, splitWidth, INTRA);
				IDCT_block(dbd, numOfblck8, blocksize2, INTRA);
				IDPCM_pix_block(dfrm, numOfblck16, numOfblck8, blocksize2, splitWidth);
			}
			mergeBlock(dbd, blocksize2, INTRA);

			intraCbCr(dfrm, dcbbd, dcrbd, blocksize2, numOfblck16, QstepDC, QstepAC);
			free(dbd.intraInverseDCTblck);
		}

		intraImgReconstruct(dfrm);

		for(int numOfblck16=0; numOfblck16<totalblck; numOfblck16++)
		{
			for(int numOfblck8=0; numOfblck8<nblckloop; numOfblck8++)
				free(dfrm.dblocks[numOfblck16].intraInverseQuanblck[numOfblck8]);
			free(dfrm.dblocks[numOfblck16].intraInverseQuanblck);
			
			free(dfrm.dcbblocks[numOfblck16].intraInverseQuanblck);
			free(dfrm.dcrblocks[numOfblck16].intraInverseQuanblck);
			free(dfrm.dcbblocks[numOfblck16].intraInverseDCTblck);
			free(dfrm.dcrblocks[numOfblck16].intraInverseDCTblck);
		}
	}
}
void intraPredictionDecode(DFrameData& dfrm, int QstepDC, int QstepAC)
{
	int totalblck   = dfrm.nblocks16;
	int nblckloop   = dfrm.nblckloop;
	int blocksize1  = dfrm.blocksize1;
	int blocksize2  = dfrm.blocksize2;
	int splitWidth  = dfrm.splitWidth;
	int splitHeight = dfrm.splitHeight;

	for(int numOfblck16=0; numOfblck16<totalblck; numOfblck16++)
	{
		DBlockData& dbd    = dfrm.dblocks[numOfblck16];
		DCBlockData& dcbbd = dfrm.dcbblocks[numOfblck16];
		DCBlockData& dcrbd = dfrm.dcrblocks[numOfblck16];
			
		/* 할당구간 */
		dbd.intraQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblckloop);
		for(int i=0; i<nblckloop; i++)
			dbd.intraQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

		dbd.intraInverseQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblckloop);
		for(int i=0; i<nblckloop; i++)
			dbd.intraInverseQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

		dbd.intraInverseDCTblck = (Block8d**)malloc(sizeof(Block8d*)*nblckloop);
		for(int i=0; i<nblckloop; i++)
			dbd.intraInverseDCTblck[i] = (Block8d*)malloc(sizeof(Block8d));

		dbd.intraRestructedblck8 = (Block8u**)malloc(sizeof(Block8u*)*nblckloop);
		for(int i=0; i<nblckloop; i++)
			dbd.intraRestructedblck8[i] = (Block8u*)malloc(sizeof(Block8u));

		dcbbd.intraInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
		dcrbd.intraInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
		dcbbd.intraInverseDCTblck  = (Block8d*)malloc(sizeof(Block8d));
		dcrbd.intraInverseDCTblck  = (Block8d*)malloc(sizeof(Block8d));

		/* 할당구간끝 */
		for(int numOfblck8=0; numOfblck8<nblckloop; numOfblck8++)
		{
			reorderingblck(dbd, numOfblck8, INTRA);
			IQuantization_block(dbd, numOfblck8, blocksize2, QstepDC, QstepAC, INTRA);
			IDPCM_DC_block(dfrm, numOfblck16, numOfblck8, blocksize2, splitWidth, INTRA);
			IDCT_block(dbd, numOfblck8, blocksize2, INTRA);
			IDPCM_pix_block(dfrm, numOfblck16, numOfblck8, blocksize2, splitWidth);
		}
		mergeBlock(dbd, blocksize2, INTRA);

		intraCbCr(dfrm, dcbbd, dcrbd, blocksize2, numOfblck16, QstepDC, QstepAC);
		free(dbd.intraInverseDCTblck);
	}

	intraImgReconstruct(dfrm);

	for(int numOfblck16=0; numOfblck16<totalblck; numOfblck16++)
	{
		for(int numOfblck8=0; numOfblck8<nblckloop; numOfblck8++)
			free(dfrm.dblocks[numOfblck16].intraInverseQuanblck[numOfblck8]);
		free(dfrm.dblocks[numOfblck16].intraInverseQuanblck);
			
		free(dfrm.dcbblocks[numOfblck16].intraInverseQuanblck);
		free(dfrm.dcrblocks[numOfblck16].intraInverseQuanblck);
		free(dfrm.dcbblocks[numOfblck16].intraInverseDCTblck);
		free(dfrm.dcrblocks[numOfblck16].intraInverseDCTblck);
	}
}
void interPredictionDecode(DFrameData& dcntFrm, DFrameData& dprevFrm, int QstepDC, int QstepAC)
{
	int totalblck = dcntFrm.nblocks16;
	int nblckloop = dcntFrm.nblckloop;
	int blocksize1 = dcntFrm.dblocks->blocksize1;
	int blocksize2 = dcntFrm.dblocks->blocksize2;
	int splitWidth = dcntFrm.splitWidth;
		

	for(int nblck=0; nblck<totalblck; nblck++)
	{
		/* 할당~ */					
		dcntFrm.dblocks[nblck].interQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblckloop);
		for(int i=0; i<nblckloop; i++)
			dcntFrm.dblocks[nblck].interQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

		dcntFrm.dblocks[nblck].interInverseQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblckloop);
		for(int i=0; i<nblckloop; i++)
			dcntFrm.dblocks[nblck].interInverseQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

		dcntFrm.dblocks[nblck].interInverseDCTblck = (Block8d**)malloc(sizeof(Block8d*)*nblckloop);
		for(int i=0; i<nblckloop; i++)
			dcntFrm.dblocks[nblck].interInverseDCTblck[i] = (Block8d*)malloc(sizeof(Block8d));

		dcntFrm.dblocks[nblck].interInverseErrblck16 = (Block16i*)malloc(sizeof(Block16i));
		/* 할당 끝*/

		for(int numOfblck8=0; numOfblck8<nblckloop; numOfblck8++)
		{
			reorderingblck(dcntFrm.dblocks[nblck], numOfblck8, INTER);
			IQuantization_block(dcntFrm.dblocks[nblck], numOfblck8, blocksize2, QstepDC, QstepAC, INTER);
			IDPCM_DC_block(dcntFrm, nblck, numOfblck8, blocksize2, splitWidth, INTER);
			IDCT_block(dcntFrm.dblocks[nblck], numOfblck8, blocksize2, INTER); // 복호된 err값을 반환
		}
		ImvPrediction(dcntFrm, nblck);
		mergeBlock(dcntFrm.dblocks[nblck], blocksize2, INTER);		

		/* free */
		free(dcntFrm.dblocks[nblck].interQuanblck);
		free(dcntFrm.dblocks[nblck].interInverseDCTblck);
	}
		
	interYReconstruct(dcntFrm, dprevFrm);
	interCbCr(dcntFrm, dprevFrm, QstepDC, QstepAC);
	
	for(int nblck=0; nblck<totalblck; nblck++)
	{
		for(int i=0; i<nblckloop; i++)
			free(dcntFrm.dblocks[nblck].interInverseQuanblck[i]);
		free(dcntFrm.dblocks[nblck].interInverseQuanblck);
		free(dcntFrm.dblocks[nblck].interInverseErrblck16);
	}
}

int MVientropy(unsigned char* bitchar, MotionVector& dmv)
{
	int cntidx   = 0;
	int val		 = 0;
	int signidx  = 0;
	int rangelen = 0;
	int rangeidx = 0;
	int codeword = 0;
	int temp	 = 0;
	int len      = 0;
	int last     = 0;

	if(bitchar[0]==0 && bitchar[1]==0) // cate 0: 0
	{
		len = 2;
		val = 0;
	}
	else if(bitchar[0]==0 && bitchar[1]==1 && bitchar[02]==0) // cate 1: 1
	{
		len = 4;
		signidx=3;
		val = (bitchar[signidx]==1)? 1:-1;
	}
	else if(bitchar[0]==0 && bitchar[1]==1 && bitchar[2]==1) // cate 2: 2~3
	{
		len=5; codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[0]==1 && bitchar[1]==0 && bitchar[2]==0) // cate 3: 4~7
	{
		len=6;codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 4+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[0]==1 && bitchar[1]==0 && bitchar[2]==1) // cate 4: 8~15
	{
		len=7;codeword=3;
		signidx=3;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 8+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[0]==1 && bitchar[1]==1 && bitchar[2]==0) // cate 5: 16~31
	{
		len=8;codeword=3;
		signidx=3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 16+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 6: 32~63
		bitchar[last+3]==0) 
	{
		len=10;codeword=4;
		signidx=last+codeword;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 32+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 7: 64~127
		bitchar[last+3]==1 && bitchar[last+4]==0) 
	{
		len=12;codeword=5;
		signidx=last+5;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 64+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 8: 128~255
		bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==0) 
	{
		len=14;codeword=6;
		signidx=last+6;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 128+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 9: 256~511
		bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
		bitchar[last+6]==0)
	{
		len=16;codeword=7;
		signidx=last+7;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 256+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 10: 512~1023
		bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 && 
		bitchar[last+6]==1 && bitchar[last+7]==0) 
	{
		len=18;codeword=8;
		signidx=last+8;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 512+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 11: 1024~2047
		bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
		bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==0) 
	{
		len=20;codeword=9;
		signidx=last+9;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 1024+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 12: 2048
		bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
		bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==1 && bitchar[last+9]==0) 
	{
		len=22;codeword=10;
		signidx=last+10;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2048+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}


	last = len;
	dmv.x = val;
	//cout << "mv x bits: " << last << endl;

	if(bitchar[last]==0 && bitchar[last+1]==0) // cate 0: 0
	{
		len=2;
		val=0;
	}
	else if(bitchar[last]==0 && bitchar[last+1]==1 && bitchar[last+2]==0) // cate 1: 1
	{
		len=4;
		signidx=last+3;
		val = (bitchar[signidx]==1)? 1:-1;
	}
	else if(bitchar[last]==0 && bitchar[last+1]==1 && bitchar[last+2]==1) // cate 2: 2~3
	{
		len=5; codeword=3;
		signidx=last+3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[last]==1 && bitchar[last+1]==0 && bitchar[last+2]==0) // cate 3: 4~7
	{
		len=6;codeword=3;
		signidx=last+3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 4+temp;
		val = (bitchar[signidx]==1)? val:-val;

	}
	else if(bitchar[last]==1 && bitchar[last+1]==0 && bitchar[last+2]==1) // cate 4: 8~15
	{
		len=7;codeword=3;
		signidx=last+3;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 8+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]==1 && bitchar[last+1]==1 && bitchar[last+2]==0) // cate 5: 16~31
	{
		len=8;codeword=3;
		signidx=last+3;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 16+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 6: 32~63
		bitchar[last+3]==0) 
	{
		len=10;codeword=4;
		signidx=last+codeword;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 32+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 7: 64~127
		bitchar[last+3]==1 && bitchar[last+4]==0) 
	{
		len=12;codeword=5;
		signidx=last+5;
		temp=0; rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 64+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 8: 128~255
		bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==0) 
	{
		len=14;codeword=6;
		signidx=last+6;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 128+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 9: 256~511
		bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
		bitchar[last+6]==0)
	{
		len=16;codeword=7;
		signidx=last+7;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 256+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 10: 512~1023
		bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 && 
		bitchar[last+6]==1 && bitchar[last+7]==0) 
	{
		len=18;codeword=8;
		signidx=last+8;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 512+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 11: 1024~2047
		bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
		bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==0) 
	{
		len=20;codeword=9;
		signidx=last+9;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 1024+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	else if(bitchar[last]  ==1 && bitchar[last+1]==1 && bitchar[last+2]==1 && // cate 12: 2048
		bitchar[last+3]==1 && bitchar[last+4]==1 && bitchar[last+5]==1 &&
		bitchar[last+6]==1 && bitchar[last+7]==1 && bitchar[last+8]==1 && bitchar[last+9]==0) 
	{
		len=22;codeword=10;
		signidx=last+10;
		temp=0;rangeidx=signidx+1;
		rangelen=len-codeword-1;
		for(int n=0; n<rangelen; n++)
		{
			temp |= bitchar[rangeidx+n];
			if(n!=rangelen-1) temp <<= 1;				
		}
		val = 2048+temp;
		val = (bitchar[signidx]==1)? val:-val;
	}
	
	dmv.y = val;
	cntidx = last + len;
	//cout << "mv y bits: " << len << endl;
	return cntidx;
}
void interCbCr(DFrameData& dcntFrm, DFrameData& dprevFrm, int QstepDC, int QstepAC)
{
	int totalblck = dcntFrm.CbCrSplitHeight*dcntFrm.CbCrSplitWidth;
	int blocksize = dcntFrm.dcbblocks->blocksize;

	// motionEstimation -> dont need ME because CbCr motion vector can be gotten by Y motion vector / 2
	for(int i=0; i<totalblck; i++)
	{
		dcntFrm.dcbblocks[i].interInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
		dcntFrm.dcrblocks[i].interInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
		dcntFrm.dcbblocks[i].interInverseDCTblck = (Block8d*)malloc(sizeof(Block8d));
		dcntFrm.dcrblocks[i].interInverseDCTblck = (Block8d*)malloc(sizeof(Block8d));
	}

	// 여기서부터 블록단위 반복 
	for(int nblck=0; nblck<totalblck; nblck++)
	{
		// Cb 할당 //
		dcntFrm.dcbblocks[nblck].interQuanblck = (Block8i*)malloc(sizeof(Block8i));

		Creorderingblck(dcntFrm.dcbblocks[nblck], INTER);
		CIQuantization_block(dcntFrm.dcbblocks[nblck], blocksize, QstepDC, QstepAC, INTER);
		CIDPCM_DC_block(dcntFrm, dcntFrm.dcbblocks[nblck], nblck, CB, INTER);
		CIDCT_block(dcntFrm.dcbblocks[nblck], blocksize, INTER);


		// Cr 할당 //
		dcntFrm.dcrblocks[nblck].interQuanblck = (Block8i*)malloc(sizeof(Block8i));

		Creorderingblck(dcntFrm.dcrblocks[nblck], INTER);
		CIQuantization_block(dcntFrm.dcrblocks[nblck], blocksize, QstepDC, QstepAC, INTER);
		CIDPCM_DC_block(dcntFrm, dcntFrm.dcrblocks[nblck], nblck, CR, INTER);
		CIDCT_block(dcntFrm.dcrblocks[nblck], blocksize, INTER);
	}

	interCbCrReconstruct(dcntFrm, dprevFrm);

	for(int i=0; i<totalblck; i++)
	{
		free(dcntFrm.dcbblocks[i].interInverseQuanblck);
		free(dcntFrm.dcrblocks[i].interInverseQuanblck);
		free(dcntFrm.dcbblocks[i].interInverseDCTblck);
		free(dcntFrm.dcrblocks[i].interInverseDCTblck);
	}
}
void interCbCrReconstruct(DFrameData& dcntFrm, DFrameData& dprevFrm)
{
	int blocksize   = dcntFrm.dcbblocks->blocksize;
	int totalblck   = dcntFrm.nblocks16;
	int splitWidth  = dcntFrm.CbCrSplitWidth;
	int splitHeight = dcntFrm.CbCrSplitHeight;
	int width		= splitWidth*blocksize;		// 176
	int height		= splitHeight*blocksize;	// 144
		
	int padlen		 = dcntFrm.dcbblocks->blocksize; // 8
	int padImgWidth  = width  + padlen * 2;	//192
	int padImgHeight = height + padlen * 2;	//160	
		
	// 패딩 이미지 생성
	unsigned char* paddingImageCb = (unsigned char*) calloc(sizeof(unsigned char), padImgWidth*padImgHeight);
	getPaddingImage(dprevFrm.decodedCb, paddingImageCb, padImgWidth, padlen, width, height);
	dcntFrm.decodedCb = (unsigned char*) malloc(sizeof(unsigned char)*width*height);

	unsigned char* paddingImageCr = (unsigned char*) calloc(sizeof(unsigned char), padImgWidth*padImgHeight);
	getPaddingImage(dprevFrm.decodedCr, paddingImageCr, padImgWidth, padlen, width, height);
	dcntFrm.decodedCr = (unsigned char*) malloc(sizeof(unsigned char)*width*height);

	unsigned char* Cbchannel = (unsigned char*)calloc(width*height, sizeof(unsigned char));
	unsigned char* Crchannel = (unsigned char*)calloc(width*height, sizeof(unsigned char));

	int tempCb=0, tempCr=0;
	int refx=0, refy=0, cntx=0, cnty=0;
	int cntindex=0, refindex=0;
	for(int nblck=0; nblck<totalblck; nblck++)
	{
		DCBlockData &cntcbbd  = dcntFrm.dcbblocks[nblck];
		DCBlockData &prevcbbd = dprevFrm.dcbblocks[nblck];
		DCBlockData &cntcrbd  = dcntFrm.dcrblocks[nblck];
		DCBlockData &prevcrbd = dprevFrm.dcrblocks[nblck];

		cntx = (nblck%splitWidth)*blocksize;
		cnty = (nblck/splitWidth)*blocksize;
		refx = cntx - (dcntFrm.dblocks[nblck].Reconstructedmv.x/2) + padlen;
		refy = cnty - (dcntFrm.dblocks[nblck].Reconstructedmv.y/2) + padlen;

		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				cntindex=((y*width)+(cnty*width))+(cntx)+ x;
				refindex=((y*padImgWidth)+(refy*padImgWidth))+(refx)+ x;
				tempCb = paddingImageCb[refindex] + cntcbbd.interInverseDCTblck->block[y][x];
				tempCb = (tempCb>255) ? 255:tempCb;
				tempCb = (tempCb<0)   ? 0  :tempCb;
				Cbchannel[cntindex] = tempCb;

				tempCr = paddingImageCr[refindex] + cntcrbd.interInverseDCTblck->block[y][x];
				tempCr = (tempCr>255) ? 255:tempCr;
				tempCr = (tempCr<0)   ? 0  :tempCr;
				Crchannel[cntindex] = tempCr;
			}
		}			
	}
	memcpy(dcntFrm.decodedCb, Cbchannel, sizeof(unsigned char)*width*height);
	memcpy(dcntFrm.decodedCr, Crchannel, sizeof(unsigned char)*width*height);

	free(paddingImageCb);
	free(paddingImageCr);
	free(Cbchannel);
	free(Crchannel);
}


/* Decodeing function */
void reorderingblck(DBlockData& dbd, int numOfblck8, int predmode)
{
	int blocksize = dbd.blocksize2;
	Block8i *Quanblck;
	int* reorderedblck;
	
	if(predmode==INTRA)		 
	{
		Quanblck = (dbd.intraQuanblck[numOfblck8]);
		reorderedblck = dbd.intraReorderedblck[numOfblck8];
	}
	else if(predmode==INTER) 
	{
		Quanblck = (dbd.interQuanblck[numOfblck8]);
		reorderedblck = dbd.interReorderedblck[numOfblck8];
	}
	
	izigzagScanning(reorderedblck, *Quanblck, blocksize);

	//cout << "reordering blck";
	//for(int i=0; i<blocksize*blocksize; i++)
	//{
	//	if(i%blocksize==0)
	//		cout << endl;
	//	cout << reorderedblck[i] << " ";		
	//}
	//cout << endl;
	//cout << endl;

	//cout << "Quanblck" << endl;
	//for(int y=0; y<blocksize; y++)
	//{
	//	for(int x=0; x<blocksize; x++)
	//	{
	//		cout << Quanblck->block[y][x] << " ";
	//	}
	//	cout << endl;
	//}
	//system("pause");
	free(reorderedblck);	
}
void Creorderingblck(DCBlockData& dcbd, int predmode)
{
	int blocksize = dcbd.blocksize;
	Block8i *Quanblck;
	int* reorderedblck;
	
	if(predmode==INTRA)		 
	{
		Quanblck = dcbd.intraQuanblck;
		reorderedblck = dcbd.intraReorderedblck;
	}
	else if(predmode==INTER) 
	{
		Quanblck = dcbd.interQuanblck;
		reorderedblck = dcbd.interReorderedblck;
	}
		
	izigzagScanning(reorderedblck, *Quanblck, blocksize);
	free(reorderedblck);	
}
void izigzagScanning(int* src, Block8i& dst, int blocksize)
{
	int beginidx=0;
	int nloop=1;

	/*for(int nloop=1; nloop<=7; nloop+=2)
	{
		izzf(src, dst, nloop, beginidx, blocksize);
		beginidx+=nloop*2+1;
	}

	for(int nloop=6; nloop>=0; nloop-=2)
	{
		izzf(src, dst, nloop, beginidx, blocksize);
		beginidx+=nloop*2+1;
	}*/

	dst.block[0][0] = src[0];
	dst.block[0][1] = src[1];
	dst.block[1][0] = src[2];
	dst.block[2][0] = src[3];
	dst.block[1][1] = src[4];
	dst.block[0][2] = src[5];
	dst.block[0][3] = src[6];
	dst.block[1][2] = src[7];
	dst.block[2][1] = src[8];
	dst.block[3][0] = src[9];
	dst.block[4][0] = src[10];
	dst.block[3][1] = src[11];
	dst.block[2][2] = src[12];
	dst.block[1][3] = src[13];
	dst.block[0][4] = src[14];
	dst.block[0][5] = src[15];
	dst.block[1][4] = src[16];
	dst.block[2][3] = src[17];
	dst.block[3][2] = src[18];
	dst.block[4][1] = src[19];
	dst.block[5][0] = src[20];
	dst.block[6][0] = src[21];
	dst.block[5][1] = src[22];
	dst.block[4][2] = src[23];
	dst.block[3][3] = src[24];
	dst.block[2][4] = src[25];
	dst.block[1][5] = src[26];
	dst.block[0][6] = src[27];
	dst.block[0][7] = src[28];
	dst.block[1][6] = src[29];
	dst.block[2][5] = src[30];
	dst.block[3][4] = src[31];
	dst.block[4][3] = src[32];
	dst.block[5][2] = src[33];
	dst.block[6][1] = src[34];
	dst.block[7][0] = src[35];


	dst.block[7][1] = src[36];
	dst.block[6][2] = src[37];
	dst.block[5][3] = src[38];
	dst.block[4][4] = src[39];
	dst.block[3][5] = src[40];
	dst.block[2][6] = src[41];
	dst.block[1][7] = src[42];
	dst.block[2][7] = src[43];
	dst.block[3][6] = src[44];
	dst.block[4][5] = src[45];
	dst.block[5][4] = src[46];
	dst.block[6][3] = src[47];
	dst.block[7][2] = src[48];
	dst.block[7][3] = src[49];
	dst.block[6][4] = src[50];
	dst.block[5][5] = src[51];
	dst.block[4][6] = src[52];
	dst.block[3][7] = src[53];
	dst.block[4][7] = src[54];
	dst.block[5][6] = src[55];
	dst.block[6][5] = src[56];
	dst.block[7][4] = src[57];
	dst.block[7][5] = src[58];
	dst.block[6][6] = src[59];
	dst.block[5][7] = src[60];
	dst.block[6][7] = src[61];
	dst.block[7][6] = src[62];
	dst.block[7][7] = src[63];
}
void izzf(int* src, Block8i& dst, int nloop, int beginidx, int blocksize)
{
	int len = blocksize-1;
	if(nloop%2)
	{   //상삼각?
		for(int i=0; i<nloop; i++)  dst.block[nloop-i-1][i] = 0;//src[beginidx+i];
		dst.block[0][nloop] = 0;//src[beginidx+nloop];
		for(int i=0; i<nloop; i++) dst.block[nloop-i][i] = 0;//src[(beginidx+nloop*2)-i];
	}
	else
	{	//하삼각?
		for(int i=0; i<nloop; i++) dst.block[len-i][len-nloop+i] = 0;//src[beginidx+i];
		dst.block[len][len-nloop] = 0;//src[beginidx+nloop];
		for(int i=0; i<nloop; i++) dst.block[len-i][len-nloop+i+1] = 0;//src[(beginidx+nloop*2)-i];
	}
}
void IQuantization_block(DBlockData &dbd, int numOfblck8, int blocksize, int QstepDC, int QstepAC, int type)
{
	Block8i *Quanblck;
	Block8i *IQuanblck;
	if(type==INTRA)
	{
		Quanblck  = (dbd.intraQuanblck[numOfblck8]);
		IQuanblck = (dbd.intraInverseQuanblck[numOfblck8]);
	}
	else if(type==INTER)
	{
		Quanblck  = (dbd.interQuanblck[numOfblck8]);
		IQuanblck = (dbd.interInverseQuanblck[numOfblck8]);
	}

	for(int y=0; y<blocksize; y++)
		for(int x=0; x<blocksize; x++)
			IQuanblck->block[y][x] = 0;

	int Qstep=0;
	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			Qstep = (x==0&&y==0) ? QstepDC:QstepAC;
			IQuanblck->block[y][x] = Quanblck->block[y][x] * Qstep;
		}
	}

	/*cout << "Quanblck" << endl;
	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			cout << Quanblck->block[y][x] << " ";
		}
		cout << endl;
	}
	cout << endl << endl;

	cout << "IQuanblck" << endl;
	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			cout << IQuanblck->block[y][x] << " ";
		}
		cout << endl;
	}
	cout << endl << endl;
	system("pause");*/

	// data saved in Quantization aren't needed after IQuantization Processing, So free
	free(Quanblck);
}
void IDPCM_DC_block(DFrameData &frm, int numOfblck16, int numOfblck8, int blocksize, int splitWidth, int predmode)
{
	int a=0, b=0, c=0;
	int median=0;
	int numOfCurrentBlck=0;

	DBlockData& bd = frm.dblocks[numOfblck16];

	if(predmode==INTRA)
	{
		if(numOfblck16 == 0) // 16x16블록의 위치가 첫 번째이다
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// DC value - 1024
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + 1024;
				break;
			case 1:
				numOfCurrentBlck=1;
				// DC value - DC value of left block
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + bd.intraInverseQuanblck[0]->block[0][0];
				break;
			case 2:
				numOfCurrentBlck=2;
				// DC value - DC value of upper block
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + bd.intraInverseQuanblck[0]->block[0][0];
				break;
			case 3:
				numOfCurrentBlck=3;
				// DC value - median value of DC value among left, upper left, upper right
				a = bd.intraInverseQuanblck[2]->block[0][0]; // left
				b = bd.intraInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper
				if( (a>b) && (a>c))		 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			}
		}
		else if(numOfblck16/splitWidth == 0) // 16x16의 위치가 첫 행이다.
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// dc - left dc
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + frm.dblocks[numOfblck16-1].intraInverseQuanblck[1]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// dc - left dc
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + bd.intraInverseQuanblck[0]->block[0][0];
				break;
			case 2:
				numOfCurrentBlck=2;
				// median
				a = frm.dblocks[numOfblck16-1].intraInverseQuanblck[3]->block[0][0]; // left
				b = bd.intraInverseQuanblck[0]->block[0][0]; // upper 
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper right
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 3:
				numOfCurrentBlck=3;
				// median
				a = bd.intraInverseQuanblck[2]->block[0][0]; // left
				b = bd.intraInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			}
		}
		else if(numOfblck16%splitWidth == 0) // 16x16 블록의 위치가 첫 열이다.
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// upper
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + frm.dblocks[numOfblck16-splitWidth].intraInverseQuanblck[2]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// median (l u ur)
				a = bd.intraInverseQuanblck[0]->block[0][0]; // left
				b = frm.dblocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0]; // upper
				c = frm.dblocks[numOfblck16-splitWidth+1].intraInverseQuanblck[2]->block[0][0]; // upper right
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 2:
				numOfCurrentBlck=2;
				// upper
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + bd.intraInverseQuanblck[0]->block[0][0];
				break;
			case 3:
				numOfCurrentBlck=3;
				// median (l ul u)
				a = bd.intraInverseQuanblck[2]->block[0][0]; // left
				b = bd.intraInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			}
		}
		else // 16x16 블록이 그 외의 위치에 있다.
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// median (l u ur)
				a = frm.dblocks[numOfblck16-1].intraInverseQuanblck[1]->block[0][0];
				b = frm.dblocks[numOfblck16-splitWidth].intraInverseQuanblck[2]->block[0][0];
				c = frm.dblocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0];
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 1:
				numOfCurrentBlck=1;
				// median (l u ur); 끝 블럭은 (l ul u)으로 해줘야 됨
				if(numOfblck16%splitWidth == splitWidth-1)
				{
					a = bd.intraInverseQuanblck[0]->block[0][0];	// left
					b = frm.dblocks[numOfblck16-splitWidth].intraInverseQuanblck[2]->block[0][0]; // upper left
					c = frm.dblocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0]; // upper 
				}
				else
				{
					a = bd.intraInverseQuanblck[0]->block[0][0];	// left
					b = frm.dblocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0]; // upper
					c = frm.dblocks[numOfblck16-splitWidth+1].intraInverseQuanblck[2]->block[0][0]; // upper right
				}
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 2:
				numOfCurrentBlck=2;
				// median (l u ur);
				a = frm.dblocks[numOfblck16-1].intraInverseQuanblck[3]->block[0][0];	// left
				b = bd.intraInverseQuanblck[0]->block[0][0];	// upper
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper left
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 3:
				numOfCurrentBlck=3;
				a = bd.intraInverseQuanblck[2]->block[0][0];	// left
				b = bd.intraInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper 
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			}
		}
	}
	else if(predmode==INTER)
	{
		if(numOfblck16 == 0) // 16x16블록의 위치가 첫 번째이다
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// DC value - 1024
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + 1024;
				break;
			case 1:
				numOfCurrentBlck=1;
				// DC value - DC value of left block
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + bd.interInverseQuanblck[0]->block[0][0];
				break;
			case 2:
				numOfCurrentBlck=2;
				// DC value - DC value of upper block
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + bd.interInverseQuanblck[0]->block[0][0];
				break;
			case 3:
				numOfCurrentBlck=3;
				// DC value - median value of DC value among left, upper left, upper right
				a = bd.interInverseQuanblck[2]->block[0][0]; // left
				b = bd.interInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper
				if( (a>b) && (a>c))		 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			}
		}
		else if(numOfblck16/splitWidth == 0) // 16x16의 위치가 첫 행이다.
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// dc - left dc
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + frm.dblocks[numOfblck16-1].interInverseQuanblck[1]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// dc - left dc
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + bd.interInverseQuanblck[0]->block[0][0];
				break;
			case 2:
				numOfCurrentBlck=2;
				// median
				a = frm.dblocks[numOfblck16-1].interInverseQuanblck[3]->block[0][0]; // left
				b = bd.interInverseQuanblck[0]->block[0][0]; // upper 
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper right
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 3:
				numOfCurrentBlck=3;
				// median
				a = bd.interInverseQuanblck[2]->block[0][0]; // left
				b = bd.interInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			}
		}
		else if(numOfblck16%splitWidth == 0) // 16x16 블록의 위치가 첫 열이다.
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// upper
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + frm.dblocks[numOfblck16-splitWidth].interInverseQuanblck[2]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// median (l u ur)
				a = bd.interInverseQuanblck[0]->block[0][0]; // left
				b = frm.dblocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0]; // upper
				c = frm.dblocks[numOfblck16-splitWidth+1].interInverseQuanblck[2]->block[0][0]; // upper right
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 2:
				numOfCurrentBlck=2;
				// upper
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + bd.interInverseQuanblck[0]->block[0][0];
				break;
			case 3:
				numOfCurrentBlck=3;
				// median (l ul u)
				a = bd.interInverseQuanblck[2]->block[0][0]; // left
				b = bd.interInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			}
		}
		else // 16x16 블록이 그 외의 위치에 있다.
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// median (l u ur)
				a = frm.dblocks[numOfblck16-1].interInverseQuanblck[1]->block[0][0];
				b = frm.dblocks[numOfblck16-splitWidth].interInverseQuanblck[2]->block[0][0];
				c = frm.dblocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0];
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 1:
				numOfCurrentBlck=1;
				// median (l u ur); 끝 블럭은 (l ul u)으로 해줘야 됨
				if(numOfblck16%splitWidth == splitWidth-1)
				{
					a = bd.interInverseQuanblck[0]->block[0][0];	// left
					b = frm.dblocks[numOfblck16-splitWidth].interInverseQuanblck[2]->block[0][0]; // upper left
					c = frm.dblocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0]; // upper 
				}
				else
				{
					a = bd.interInverseQuanblck[0]->block[0][0];	// left
					b = frm.dblocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0]; // upper
					c = frm.dblocks[numOfblck16-splitWidth+1].interInverseQuanblck[2]->block[0][0]; // upper right
				}
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 2:
				numOfCurrentBlck=2;
				// median (l u ur);
				a = frm.dblocks[numOfblck16-1].interInverseQuanblck[3]->block[0][0];	// left
				b = bd.interInverseQuanblck[0]->block[0][0];	// upper
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper left
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 3:
				numOfCurrentBlck=3;
				a = bd.interInverseQuanblck[2]->block[0][0];	// left
				b = bd.interInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper 
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			}
		}
	}
}
void IDCT_block(DBlockData &bd, int numOfblck8, int blocksize, int predmode)
{
	Block8d *IDCTblck;
	Block8i *IQuanblck;
	Block8d temp;
	if(predmode==INTRA)
	{
		IDCTblck  = (bd.intraInverseDCTblck[numOfblck8]);
		IQuanblck = (bd.intraInverseQuanblck[numOfblck8]);
	}
	else if(predmode==INTER)
	{
		IDCTblck  = (bd.interInverseDCTblck[numOfblck8]);
		IQuanblck = (bd.interInverseQuanblck[numOfblck8]);
	}

	double *Cu = (double *)malloc(sizeof(double)*blocksize);
	double *Cv = (double *)malloc(sizeof(double)*blocksize);

	Cu[0] = Cv[0] = irt2;
	for(int i=1; i<blocksize; i++)
	{
		Cu[i] = Cv[i] = 1.;
	}

	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			IDCTblck->block[y][x] = temp.block[y][x] = 0;
		}			
	}

	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			for(int u=0; u<blocksize; u++)
			{
				temp.block[y][x] += Cu[u] * (double)IQuanblck->block[y][u] * costable[u][x];//cos(((2*x+1)*u*pi)/16.) * cos(((2*y+1)*v*pi)/16.);
			}
		}
	}

	for(int x=0; x<blocksize; x++)
	{	
		for(int y=0; y<blocksize; y++)
		{		
			for(int v=0; v<blocksize; v++)
			{
				IDCTblck->block[y][x] += Cv[v] * temp.block[v][x] * costable[v][y];//cos(((2*x+1)*u*pi)/16.) * cos(((2*y+1)*v*pi)/16.);
			}
		}
	}

	for(int i=0; i<blocksize; i++)
	{
		for(int j=0; j<blocksize; j++)
		{
			IDCTblck->block[i][j] *= (1./4.);
		}
	}


	//Block8d idct2blck;
	//for(int y=0; y<blocksize; y++)
	//	for(int x=0; x<blocksize; x++)
	//		idct2blck.block[y][x] = 0;
	//	
	//for(int y=0; y<blocksize; y++)
	//{
	//	for(int x=0; x<blocksize; x++)
	//	{
	//		for(int v=0; v<blocksize; v++)
	//		{
	//			for(int u=0; u<blocksize; u++)
	//			{
	//				idct2blck.block[y][x] += Cu[u]*Cv[v] * (double)IQuanblck->block[v][u] * cos(((2*x+1)*u*pi)/16.) * cos(((2*y+1)*v*pi)/16.);   // 실제로는 bd.intraDCTblck[n].block[u][v]이 아니라 InverseQuanblck가 맞음
	//			}
	//		}
	//	}
	//}
	//for(int y=0; y<blocksize; y++)
	//{
	//	for(int x=0; x<blocksize; x++)
	//	{
	//		idct2blck.block[y][x] *= (1./4.);
	//	}
	//}

	/*cout << "idct2" << endl;
	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			printf("%.2lf ", idct2blck.block[y][x]);
		}
		cout << endl;
	}
	cout << endl;*/
	/*cout << "idct1" << endl;
	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			printf("%.2lf ", IDCTblck->block[y][x]);
		}
		cout << endl;
	}
	system("pause");*/

	free(Cv);
	free(Cu);
	//free(IQuanblck);
}
void IDPCM_pix_block(DFrameData &frm, int numOfblck16, int numOfblck8, int blocksize, int splitWidth)
{
	int numOfCurrentBlck = 0;
	int predMode = 0;
	int modeblck0=0, modeblck1=0, modeblck2=0;
	int modetemp0=0, modetemp1=0, modetemp2=0;
	int median   = 0;
	DBlockData &bd = frm.dblocks[numOfblck16];
	if(numOfblck16==0) // 16x16 첫번째 블록
	{
		switch(numOfblck8)
		{
		case 0:
			numOfCurrentBlck=0;
			predMode = 2; // 첫번쨰 블록의 DPCM mode는 무조건 2
			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)      IDPCM_pix_0(NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(NULL, NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 1:
			numOfCurrentBlck=1;
			modetemp0 = bd.IDPCMmode[0];
			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = 2;
			else
			{
				if(modetemp0==0)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;					
				else if(modetemp0==2)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}

			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(bd.intraRestructedblck8[0]->block, NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 2:
			numOfCurrentBlck=2;
			modetemp0 = bd.IDPCMmode[0];
			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = 2;
			else
			{
				if(modetemp0==0)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;					
				else if(modetemp0==2)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}

			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(NULL, bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 3:
			numOfCurrentBlck=3;
			modeblck0 = bd.IDPCMmode[0];
			modeblck1 = bd.IDPCMmode[1];
			modeblck2 = bd.IDPCMmode[2];
			if( (modeblck0>modeblck1) && (modeblck0>modeblck2) )	 median=(modeblck1>modeblck2) ? modeblck1:modeblck2;
			else if((modeblck1>modeblck0) && (modeblck1>modeblck2))	 median=(modeblck0>modeblck2) ? modeblck0:modeblck2;
			else		   											 median=(modeblck0>modeblck1) ? modeblck0:modeblck1;

			if(bd.MPMFlag[numOfCurrentBlck]==1)
			{
				predMode = median;
			}
			else
			{
				if(median==0)       predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;
				else if(median==2)  predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else				predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}
			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(bd.intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(bd.intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(bd.intraRestructedblck8[2]->block, bd.intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		}
	}
	else if(numOfblck16/splitWidth == 0) // 16x16 첫 행
	{
		switch(numOfblck8)
		{
		case 0:
			numOfCurrentBlck=0;
			modetemp0=frm.dblocks[numOfblck16-1].IDPCMmode[1];
			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = frm.dblocks[numOfblck16-1].IDPCMmode[1];
			else
			{
				if(modetemp0==0)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;					
				else if(modetemp0==2)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}

			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(frm.dblocks[numOfblck16-1].intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(frm.dblocks[numOfblck16-1].intraRestructedblck8[1]->block, NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);

			break;
		case 1:
			numOfCurrentBlck=1;
			modetemp0=bd.IDPCMmode[0];
			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = bd.IDPCMmode[0];
			else
			{
				if(modetemp0==0)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;					
				else if(modetemp0==2)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}

			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(bd.intraRestructedblck8[0]->block, NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);

			break;
		case 2:
			numOfCurrentBlck=2;
			modetemp0 = bd.IDPCMmode[0];
			modetemp1 = frm.dblocks[numOfblck16-1].IDPCMmode[1];
			modetemp2 = frm.dblocks[numOfblck16-1].IDPCMmode[3];
			if( (modetemp0>modetemp1) && (modetemp0>modetemp2) )	 median=(modetemp1>modetemp2) ? modetemp1:modetemp2;
			else if((modetemp1>modetemp0) && (modetemp1>modetemp2))	 median=(modetemp0>modetemp2) ? modetemp0:modetemp2;
			else		   											 median=(modetemp0>modetemp1) ? modetemp0:modetemp1;

			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = median;
			else
			{
				if(median==0)       predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;
				else if(median==2)  predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else				predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}

			bd.IDPCMmode[numOfCurrentBlck]=predMode;
			if(predMode==0)		 IDPCM_pix_0(bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(frm.dblocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(frm.dblocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);

			break;
		case 3:
			numOfCurrentBlck=3;
			modeblck0 = bd.IDPCMmode[0];
			modeblck1 = bd.IDPCMmode[1];
			modeblck2 = bd.IDPCMmode[2];
			if( (modeblck0>modeblck1) && (modeblck0>modeblck2) )	 median=(modeblck1>modeblck2) ? modeblck1:modeblck2;
			else if((modeblck1>modeblck0) && (modeblck1>modeblck2))	 median=(modeblck0>modeblck2) ? modeblck0:modeblck2;
			else		   											 median=(modeblck0>modeblck1) ? modeblck0:modeblck1;

			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = median;
			else
			{
				if(median==0)       predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;
				else if(median==2)  predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else				predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}

			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(bd.intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(bd.intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(bd.intraRestructedblck8[2]->block, bd.intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);			
			break;
		}
	}
	else if(numOfblck16%splitWidth == 0) // 16x16 첫 열
	{
		switch(numOfblck8)
		{
		case 0:
			numOfCurrentBlck=0;
			modetemp0 = frm.dblocks[numOfblck16-splitWidth].IDPCMmode[2];
			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = frm.dblocks[numOfblck16-splitWidth].IDPCMmode[2];
			else
			{
				if(modetemp0==0)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;					
				else if(modetemp0==2)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}


			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(frm.dblocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(NULL, frm.dblocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 1:
			numOfCurrentBlck=1;
			modetemp0 = bd.IDPCMmode[0];
			modetemp1 = frm.dblocks[numOfblck16-splitWidth].IDPCMmode[2];
			modetemp2 = frm.dblocks[numOfblck16-splitWidth].IDPCMmode[3];
			if( (modetemp0>modetemp1) && (modetemp0>modetemp2) )	 median=(modetemp1>modetemp2) ? modetemp1:modetemp2;
			else if((modetemp1>modetemp0) && (modetemp1>modetemp2))	 median=(modetemp0>modetemp2) ? modetemp0:modetemp2;
			else		   											 median=(modetemp0>modetemp1) ? modetemp0:modetemp1;

			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = median;
			else
			{
				if(median==0)       predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;
				else if(median==2)  predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else				predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}


			bd.IDPCMmode[numOfCurrentBlck] = predMode;			
			if(predMode==0)		 IDPCM_pix_0(frm.dblocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(bd.intraRestructedblck8[0]->block, frm.dblocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 2:
			numOfCurrentBlck=2;
			modetemp0 = bd.IDPCMmode[0];
			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = bd.IDPCMmode[0];
			else
			{
				if(modetemp0==0)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;					
				else if(modetemp0==2)
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else
					predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}


			bd.IDPCMmode[numOfCurrentBlck] = predMode; 
			if(predMode==0)		 IDPCM_pix_0(bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(NULL, bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 3:
			numOfCurrentBlck=3;
			modeblck0 = bd.IDPCMmode[0];
			modeblck1 = bd.IDPCMmode[1];
			modeblck2 = bd.IDPCMmode[2];
			if( (modeblck0>modeblck1) && (modeblck0>modeblck2) )	 median=(modeblck1>modeblck2) ? modeblck1:modeblck2;
			else if((modeblck1>modeblck0) && (modeblck1>modeblck2))	 median=(modeblck0>modeblck2) ? modeblck0:modeblck2;
			else		   											 median=(modeblck0>modeblck1) ? modeblck0:modeblck1;

			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = median;
			else
			{
				if(median==0)       predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;
				else if(median==2)  predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else				predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}

			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(bd.intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(bd.intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(bd.intraRestructedblck8[2]->block, bd.intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);	
			break;
		}
	}
	else // 그외의 블록
	{
		switch(numOfblck8)
		{		
		case 0:
			numOfCurrentBlck=0;
			modetemp0 = frm.dblocks[numOfblck16-1].IDPCMmode[1];
			modetemp1 = frm.dblocks[numOfblck16-splitWidth].IDPCMmode[2];
			modetemp2 = frm.dblocks[numOfblck16-splitWidth-1].IDPCMmode[3];
			if( (modetemp0>modetemp1) && (modetemp0>modetemp2) )	 median=(modetemp1>modetemp2) ? modetemp1:modetemp2;
			else if((modetemp1>modetemp0) && (modetemp1>modetemp2))	 median=(modetemp0>modetemp2) ? modetemp0:modetemp2;
			else		   											 median=(modetemp0>modetemp1) ? modetemp0:modetemp1;

			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = median;
			else
			{
				if(median==0)       predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;
				else if(median==2)  predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else				predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}

			bd.IDPCMmode[numOfCurrentBlck] = predMode;	
			if(predMode==0)		 IDPCM_pix_0(frm.dblocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(frm.dblocks[numOfblck16-1].intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(frm.dblocks[numOfblck16-1].intraRestructedblck8[1]->block, frm.dblocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 1:
			numOfCurrentBlck=1;
			modetemp0 = bd.IDPCMmode[0];
			modetemp1 = frm.dblocks[numOfblck16-splitWidth].IDPCMmode[2];
			modetemp2 = frm.dblocks[numOfblck16-splitWidth].IDPCMmode[3];
			if( (modetemp0>modetemp1) && (modetemp0>modetemp2) )	 median=(modetemp1>modetemp2) ? modetemp1:modetemp2;
			else if((modetemp1>modetemp0) && (modetemp1>modetemp2))	 median=(modetemp0>modetemp2) ? modetemp0:modetemp2;
			else		   											 median=(modetemp0>modetemp1) ? modetemp0:modetemp1;

			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = median;
			else
			{
				if(median==0)       predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;
				else if(median==2)  predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else				predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}
			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(frm.dblocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(bd.intraRestructedblck8[0]->block, frm.dblocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 2:
			numOfCurrentBlck=2;
			modetemp0 = bd.IDPCMmode[0];
			modetemp1 = frm.dblocks[numOfblck16-1].IDPCMmode[1];
			modetemp2 = frm.dblocks[numOfblck16-1].IDPCMmode[3];
			if( (modetemp0>modetemp1) && (modetemp0>modetemp2) )	 median=(modetemp1>modetemp2) ? modetemp1:modetemp2;
			else if((modetemp1>modetemp0) && (modetemp1>modetemp2))	 median=(modetemp0>modetemp2) ? modetemp0:modetemp2;
			else		   											 median=(modetemp0>modetemp1) ? modetemp0:modetemp1;

			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = median;
			else
			{
				if(median==0)       predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;
				else if(median==2)  predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else				predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}

			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(frm.dblocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(frm.dblocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 3:
			numOfCurrentBlck=3;
			modeblck0 = bd.IDPCMmode[0];
			modeblck1 = bd.IDPCMmode[1];
			modeblck2 = bd.IDPCMmode[2];
			if( (modeblck0>modeblck1) && (modeblck0>modeblck2) )	 median=(modeblck1>modeblck2) ? modeblck1:modeblck2;
			else if((modeblck1>modeblck0) && (modeblck1>modeblck2))	 median=(modeblck0>modeblck2) ? modeblck0:modeblck2;
			else		   											 median=(modeblck0>modeblck1) ? modeblck0:modeblck1;

			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = median;
			else
			{
				if(median==0)       predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?1:2;
				else if(median==2)  predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:1;
				else				predMode = (bd.intraPredMode[numOfCurrentBlck]==0)?0:2;
			}

			bd.IDPCMmode[numOfCurrentBlck] = predMode;
			if(predMode==0)		 IDPCM_pix_0(bd.intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(bd.intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(bd.intraRestructedblck8[2]->block, bd.intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);	
			break;
		}
	}

	free(bd.intraInverseDCTblck[numOfCurrentBlck]);
}
void IDPCM_pix_0(unsigned char upper[][8], double current[][8], unsigned char restored_temp[][8], int blocksize)
{	
	int temp=0;
	if(upper==NULL)
	{
		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				temp = current[y][x] + 128;
				temp = (temp>255) ? 255:temp;
				temp = (temp<0  ) ? 0  :temp;
				restored_temp[y][x] = (unsigned char)temp;
			}
		}
	}
	else
	{
		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				temp = current[y][x] + upper[blocksize-1][x];
				temp = (temp>255) ? 255:temp;
				temp = (temp<0  ) ? 0  :temp;
				restored_temp[y][x] = (unsigned char)temp;
			}
		}
	}
}
void IDPCM_pix_1(unsigned char left[][8], double current[][8], unsigned char restored_temp[][8], int blocksize)
{
	int temp=0;
	if(left==NULL)
	{
		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				temp = current[y][x] + 128;
				temp = (temp>255) ? 255:temp;
				temp = (temp<0  ) ? 0  :temp;
				restored_temp[y][x] = temp;
			}
		}
	}
	else
	{
		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				temp = current[y][x] + left[y][blocksize-1];
				temp = (temp>255) ? 255:temp;
				temp = (temp<0  ) ? 0  :temp;
				restored_temp[y][x] = temp;
				restored_temp[y][x] = temp;
			}
		}
	}
}
void IDPCM_pix_2(unsigned char left[][8], unsigned char upper[][8], double current[][8], unsigned char restored_temp[][8], int blocksize)
{
	int temp=0;
	double predValLeft  = 0;
	double predValUpper = 0;
	double predVal      = 0;

	if(left==NULL)
	{
		predValLeft = 128*8;
	}
	else
	{
		for(int i=0; i<blocksize; i++)
			predValLeft += left[i][blocksize-1];
	}

	if(upper==NULL)
	{		
		predValUpper = 128*8;
	}
	else
	{
		for(int i=0; i<blocksize; i++)
			predValUpper += upper[blocksize-1][i];
	}

	predVal = (predValLeft+predValUpper) / (blocksize+blocksize);

	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			temp = current[y][x] + predVal;
			temp = (temp>255) ? 255:temp;
			temp = (temp<0  ) ? 0  :temp;
			restored_temp[y][x]  =  (unsigned char)temp;
		}
	}
}
void mergeBlock(DBlockData &bd, int blocksize, int predmode) // 8x8 -> 16x16 으로 만들기
{	
	int nblck8 = 4;
	if(predmode==INTRA)
	{
		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				bd.intraRestructedblck16.block[y][x] = bd.intraRestructedblck8[0]->block[y][x];
			}
		}

		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				bd.intraRestructedblck16.block[y][x+blocksize] = bd.intraRestructedblck8[1]->block[y][x];
			}
		}

		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				bd.intraRestructedblck16.block[y+blocksize][x] = bd.intraRestructedblck8[2]->block[y][x];
			}
		}

		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				bd.intraRestructedblck16.block[y+blocksize][x+blocksize] = bd.intraRestructedblck8[3]->block[y][x];
			}
		}
	}
	else if(predmode==INTER)
	{
		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				bd.interInverseErrblck16->block[y][x] = (int)bd.interInverseDCTblck[0]->block[y][x];
			}
		}

		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				bd.interInverseErrblck16->block[y][x+blocksize] = (int)bd.interInverseDCTblck[1]->block[y][x];
			}
		}

		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				bd.interInverseErrblck16->block[y+blocksize][x] = (int)bd.interInverseDCTblck[2]->block[y][x];
			}
		}

		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				bd.interInverseErrblck16->block[y+blocksize][x+blocksize] = (int)bd.interInverseDCTblck[3]->block[y][x];
			}
		}		

		for(int i=0; i<nblck8; i++)
		{
			free(bd.interInverseDCTblck[i]);
		}
	}
}
void intraImgReconstruct(DFrameData &dfrm)
{
	int blocksize1  = dfrm.dblocks->blocksize1;
	int blocksize2  = dfrm.dblocks->blocksize2;
	int totalblck   = dfrm.nblocks16;
	int nblck8      = dfrm.nblocks8;
	int splitWidth  = dfrm.splitWidth;
	int splitHeight = dfrm.splitHeight;
	int CbCrSplitWidth  = dfrm.CbCrSplitWidth;
	int CbCrSplitHeight = dfrm.CbCrSplitHeight;
	int CbCrWidth  = dfrm.CbCrWidth;
	int CbCrHeight = dfrm.CbCrHeight;
	int width = splitWidth*blocksize1;
	int height = splitHeight*blocksize1;

	unsigned char* Ychannel = (unsigned char*)calloc(width*height, sizeof(unsigned char));
	unsigned char* Cbchannel = (unsigned char *) calloc((width/2) * (height/2), sizeof(unsigned char));
	unsigned char* Crchannel = (unsigned char *) calloc((width/2) * (height/2), sizeof(unsigned char));

	dfrm.decodedY  = (unsigned char*) malloc(sizeof(unsigned char)*width*height);				//checkResultFrames함수에서 반환
	dfrm.decodedCb = (unsigned char*) malloc(sizeof(unsigned char)*(CbCrWidth)*(CbCrHeight));		//checkResultFrames에서 반환
	dfrm.decodedCr = (unsigned char*) malloc(sizeof(unsigned char)*(CbCrWidth)*(CbCrHeight));		//checkResultFrames에서 반환

	int temp = 0;
	int tempCb = 0;
	int tempCr = 0;
	int nblck=0;	


	nblck=0;
	for(int y_interval=0; y_interval<splitHeight; y_interval++)
	{
		for(int x_interval=0; x_interval<splitWidth; x_interval++)
		{			
			DBlockData &bd = dfrm.dblocks[nblck]; // nblck++

			for(int y=0; y<blocksize1; y++)
			{
				for(int x=0; x<blocksize1; x++)
				{
					Ychannel[(y_interval*blocksize1*width) + (y*width) + (x_interval*blocksize1)+x] = bd.intraRestructedblck16.block[y][x];
				}
			}
			nblck++;
		}		
	}	
	memcpy(dfrm.decodedY, Ychannel, sizeof(unsigned char)*width*height);

	nblck=0;
	for(int y_interval=0; y_interval<CbCrSplitHeight; y_interval++)
	{
		for(int x_interval=0; x_interval<CbCrSplitWidth; x_interval++)
		{			
			DCBlockData &dcbbd = dfrm.dcbblocks[nblck]; // nblck++
			DCBlockData &dcrbd = dfrm.dcrblocks[nblck];
			int index=0;
			for(int y=0; y<blocksize2; y++)
			{
				for(int x=0; x<blocksize2; x++)
				{							
					tempCb = (dcbbd.intraInverseDCTblck->block[y][x]>255) ? 255:dcbbd.intraInverseDCTblck->block[y][x];							
					tempCb = (tempCb<0) ? 0 :tempCb;
					index = (y_interval*blocksize2*CbCrWidth) + (y*CbCrWidth) + (x_interval*blocksize2)+x;
					Cbchannel[index] = tempCb;

					tempCr = (dcrbd.intraInverseDCTblck->block[y][x]>255) ? 255:dcrbd.intraInverseDCTblck->block[y][x];
					tempCr = (tempCr<0) ? 0:tempCr;
					Crchannel[index] = tempCr;
				}
			}
			nblck++;
		}		
	}	
	memcpy(dfrm.decodedCb, Cbchannel, sizeof(unsigned char)*(CbCrWidth)*(CbCrHeight));
	memcpy(dfrm.decodedCr, Crchannel, sizeof(unsigned char)*(CbCrWidth)*(CbCrHeight));

	free(Ychannel);
	free(Cbchannel);
	free(Crchannel);
}
void intraCbCr(DFrameData& dfrm, DCBlockData &dcbbd, DCBlockData &dcrbd, int blocksize, int numOfblck8, int QstepDC, int QstepAC)
{
	dcbbd.intraQuanblck = (Block8i*)malloc(sizeof(Block8i));
	Creorderingblck(dcbbd, INTRA);
	CIQuantization_block(dcbbd, blocksize, QstepDC, QstepAC, INTRA);
	CIDPCM_DC_block(dfrm, dcbbd, numOfblck8, CB, INTRA);
	CIDCT_block(dcbbd, blocksize, INTRA);

	dcrbd.intraQuanblck = (Block8i*)malloc(sizeof(Block8i)*blocksize*blocksize);
	Creorderingblck(dcrbd, INTRA);
	CIQuantization_block(dcrbd, blocksize, QstepDC, QstepAC, INTRA);
	CIDPCM_DC_block(dfrm, dcrbd, numOfblck8, CR, INTRA);
	CIDCT_block(dcrbd, blocksize, INTRA);
}
void CIQuantization_block(DCBlockData& dcbd, int blocksize, int QstepDC, int QstepAC, int predmode)
{
	Block8i *IQuanblck;
	Block8i *Quanblck;
	if(predmode==INTRA)
	{
		IQuanblck = (dcbd.intraInverseQuanblck);
		Quanblck = (dcbd.intraQuanblck);
	}
	else if(predmode==INTER)
	{
		IQuanblck = (dcbd.interInverseQuanblck);
		Quanblck = (dcbd.interQuanblck);
	}
	for(int y=0; y<blocksize; y++)
		for(int x=0; x<blocksize; x++)
			IQuanblck->block[y][x] = 0;

	int Qstep = 0;
	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			Qstep = (x==0&&y==0)? QstepDC:QstepAC;
			IQuanblck->block[y][x] = Quanblck->block[y][x] * Qstep;
		}
	}

	free(Quanblck);
}
void CIDPCM_DC_block(DFrameData& dfrm, DCBlockData& dcbd, int numOfblck8, int CbCrtype, int predmode)
{
	int splitWidth			  = dfrm.CbCrSplitWidth;
	int numOfCurrentBlck	  = 0;
	int a=0, b=0, c=0, median = 0;

	DCBlockData* cbd;
	if(CbCrtype==CB) cbd = dfrm.dcbblocks;
	else if(CbCrtype==CR) cbd = dfrm.dcrblocks;

	Block8i *IQuanblck;
	Block8i *IQuanblckLeft;
	Block8i *IQuanblckUpper;
	Block8i *IQuanblckUpperLeft;
	Block8i *IQuanblckUpperRight;
	if(predmode==INTRA)
	{
		if(numOfblck8==0)
			IQuanblck = (dcbd.intraInverseQuanblck);
		else if(numOfblck8/splitWidth==0)
		{
			IQuanblck = (dcbd.intraInverseQuanblck);
			IQuanblckLeft = (cbd[numOfblck8-1].intraInverseQuanblck);
		}
		else if(numOfblck8%splitWidth==0)
		{
			IQuanblck = (dcbd.intraInverseQuanblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].intraInverseQuanblck);
		}
		else
		{
			IQuanblck = (dcbd.intraInverseQuanblck);
			IQuanblckLeft = (cbd[numOfblck8-1].intraInverseQuanblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].intraInverseQuanblck);
			IQuanblckUpperLeft = (cbd[numOfblck8-splitWidth-1].intraInverseQuanblck);
			IQuanblckUpperRight = (cbd[numOfblck8-splitWidth+1].intraInverseQuanblck);
		}
	}
	else if(predmode==INTER)
	{
		if(numOfblck8==0)
			IQuanblck = (dcbd.interInverseQuanblck);
		else if(numOfblck8/splitWidth==0)
		{
			IQuanblck = (dcbd.interInverseQuanblck);
			IQuanblckLeft = (cbd[numOfblck8-1].interInverseQuanblck);
		}
		else if(numOfblck8%splitWidth==0)
		{
			IQuanblck = (dcbd.interInverseQuanblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].interInverseQuanblck);
		}
		else
		{
			IQuanblck = (dcbd.interInverseQuanblck);
			IQuanblckLeft = (cbd[numOfblck8-1].interInverseQuanblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].interInverseQuanblck);
			IQuanblckUpperLeft = (cbd[numOfblck8-splitWidth-1].interInverseQuanblck);
			IQuanblckUpperRight = (cbd[numOfblck8-splitWidth+1].interInverseQuanblck);
		}
	}

	if(numOfblck8==0)
	{
		IQuanblck->block[0][0] = IQuanblck->block[0][0] + 1024;
	}
	else if(numOfblck8/splitWidth==0) // 첫 행
	{
		IQuanblck->block[0][0] = IQuanblck->block[0][0] + IQuanblckLeft->block[0][0];
	}
	else if(numOfblck8%splitWidth==0) // 첫 열
	{
		IQuanblck->block[0][0] = IQuanblck->block[0][0] + IQuanblckUpper->block[0][0];
	}
	else
	{
		// median
		if(numOfblck8%splitWidth == splitWidth-1)
		{
			a = IQuanblckLeft->block[0][0];		 // left
			b = IQuanblckUpperLeft->block[0][0]; // upper left
			c = IQuanblckUpper->block[0][0];	 // upper
		}
		else
		{
			a = IQuanblckLeft->block[0][0];		    // left
			b = IQuanblckUpper->block[0][0];		// upper 
			c = IQuanblckUpperRight->block[0][0];   // upper right
		}
		if((a>b)&&(a>c))		median=(b>c)?b:c;
		else if((b>a)&&(b>c))   median=(a>c)?a:c;
		else					median=(a>b)?a:b;
		IQuanblck->block[0][0] = IQuanblck->block[0][0] + median;
	}

}
void CIDCT_block(DCBlockData &dcbd, int blocksize, int predmode)
{
	Block8d *IDCTblck;
	Block8i *IQuanblck;
	Block8d temp;
	if(predmode==INTRA)
	{
		IDCTblck = (dcbd.intraInverseDCTblck);
		IQuanblck = (dcbd.intraInverseQuanblck);
	}
	else if(predmode==INTER)
	{
		IDCTblck  = (dcbd.interInverseDCTblck);
		IQuanblck = (dcbd.interInverseQuanblck);
	}

	double *Cu = (double *)malloc(sizeof(double)*blocksize);
	double *Cv = (double *)malloc(sizeof(double)*blocksize);

	Cu[0] = Cv[0] = irt2;
	for(int i=1; i<blocksize; i++)
	{
		Cu[i] = Cv[i] = 1.;
	}

	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			IDCTblck->block[y][x] = temp.block[y][x] = 0;
		}			
	}


	//for(int y=0; y<blocksize; y++)
	//{
	//	for(int x=0; x<blocksize; x++)
	//	{
	//		for(int v=0; v<blocksize; v++)
	//		{
	//			for(int u=0; u<blocksize; u++)
	//			{
	//				IDCTblck->block[y][x] += Cu[u]*Cv[v] * (double)IQuanblck->block[v][u] * cos(((2*x+1)*u*pi)/16.) * cos(((2*y+1)*v*pi)/16.);   // 실제로는 bd.intraDCTblck[n].block[u][v]이 아니라 InverseQuanblck가 맞음
	//			}
	//		}
	//	}
	//}

	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			for(int u=0; u<blocksize; u++)
			{
				temp.block[y][x] += Cu[u] * (double)IQuanblck->block[y][u] * costable[u][x];//cos(((2*x+1)*u*pi)/16.) * cos(((2*y+1)*v*pi)/16.);
			}
		}
	}

	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			for(int v=0; v<blocksize; v++)
			{
				IDCTblck->block[y][x] += Cv[v] * temp.block[v][x] * costable[v][y];//cos(((2*x+1)*u*pi)/16.) * cos(((2*y+1)*v*pi)/16.);
			}
		}
	}

	for(int i=0; i<blocksize; i++)
	{
		for(int j=0; j<blocksize; j++)
		{
			IDCTblck->block[i][j] *= (1./4.);
		}
	}

	free(Cv);
	free(Cu);
}
void ImvPrediction(DFrameData& dcntFrm, int numOfblck16)
{
	int blocksize = dcntFrm.dblocks->blocksize1;
	int totalblck = dcntFrm.nblocks16;
	int splitWidth =  dcntFrm.splitWidth;
	int x1=0, x2=0, x3=0, xmedian=0;
	int y1=0, y2=0, y3=0, ymedian=0;

	DBlockData& dbd = dcntFrm.dblocks[numOfblck16];
	if(numOfblck16==0)
	{
		dbd.Reconstructedmv.x = dbd.Reconstructedmv.x+8;
		dbd.Reconstructedmv.y = dbd.Reconstructedmv.y+8;
	}
	else if(numOfblck16/splitWidth==0)
	{
		DBlockData& prevbd = dcntFrm.dblocks[numOfblck16-1];
		dbd.Reconstructedmv.x = dbd.Reconstructedmv.x+prevbd.Reconstructedmv.x;
		dbd.Reconstructedmv.y = dbd.Reconstructedmv.y+prevbd.Reconstructedmv.y;
	}
	else if(numOfblck16%splitWidth==0)
	{			
		DBlockData& prevbd = dcntFrm.dblocks[numOfblck16-splitWidth];
		dbd.Reconstructedmv.x = dbd.Reconstructedmv.x+prevbd.Reconstructedmv.x;
		dbd.Reconstructedmv.y = dbd.Reconstructedmv.y+prevbd.Reconstructedmv.y;
	}
	else
	{			
		if(numOfblck16%splitWidth==splitWidth-1)
		{
			// median l ul u
			x1 = dcntFrm.dblocks[numOfblck16-1].Reconstructedmv.x;			 // left
			x2 = dcntFrm.dblocks[numOfblck16-splitWidth-1].Reconstructedmv.x; // upper left
			x3 = dcntFrm.dblocks[numOfblck16-splitWidth].Reconstructedmv.x;   // upper

			if((x1>x2)&&(x1>x3))	  xmedian = (x2>x3)?x2:x3;
			else if((x2>x1)&&(x2>x3)) xmedian = (x1>x3)?x1:x3;
			else					  xmedian = (x1>x2)?x1:x2;

			y1 = dcntFrm.dblocks[numOfblck16-1].Reconstructedmv.y;			 // left
			y2 = dcntFrm.dblocks[numOfblck16-splitWidth-1].Reconstructedmv.y; // upper left
			y3 = dcntFrm.dblocks[numOfblck16-splitWidth].Reconstructedmv.y;   // upper

			if((y1>y2)&&(y1>y3))	  ymedian = (y2>y3)?y2:y3;
			else if((y2>y1)&&(y2>y3)) ymedian = (y1>x3)?y1:y3;
			else					  ymedian = (y1>y2)?y1:y2;
		}
		else
		{
			// median l u ur
			x1 = dcntFrm.dblocks[numOfblck16-1].Reconstructedmv.x;				// left
			x2 = dcntFrm.dblocks[numOfblck16-splitWidth].Reconstructedmv.x;		// upper
			x3 = dcntFrm.dblocks[numOfblck16-splitWidth+1].Reconstructedmv.x;    // upper right

			if((x1>x2)&&(x1>x3))	  xmedian = (x2>x3)?x2:x3;
			else if((x2>x1)&&(x2>x3)) xmedian = (x1>x3)?x1:x3;
			else					  xmedian = (x1>x2)?x1:x2;

			y1 = dcntFrm.dblocks[numOfblck16-1].Reconstructedmv.y;			   // left
			y2 = dcntFrm.dblocks[numOfblck16-splitWidth].Reconstructedmv.y;	   // upper
			y3 = dcntFrm.dblocks[numOfblck16-splitWidth+1].Reconstructedmv.y;   // upper right

			if((y1>y2)&&(y1>y3))	  ymedian = (y2>y3)?y2:y3;
			else if((y2>y1)&&(y2>y3)) ymedian = (y1>x3)?y1:y3;
			else					  ymedian = (y1>y2)?y1:y2;
		}
		dbd.Reconstructedmv.x = dbd.Reconstructedmv.x+xmedian;
		dbd.Reconstructedmv.y = dbd.Reconstructedmv.y+ymedian;
	}
}
void interYReconstruct(DFrameData& dcntFrm, DFrameData& dprevFrm)
{
	int blocksize1  = dcntFrm.dblocks->blocksize1;
	int blocksize2  = dcntFrm.dblocks->blocksize2;
	int totalblck   = dcntFrm.nblocks16;
	int splitWidth  = dcntFrm.splitWidth;
	int splitHeight = dcntFrm.splitHeight;
	int width		= splitWidth*blocksize1;
	int height		= splitHeight*blocksize1;
	int temp		= 0;

	int padlen = dcntFrm.dblocks->blocksize1;
	int padImgWidth  = width  + padlen * 2;	//384
	int padImgHeight = height + padlen * 2;	//320	

	// 패딩 이미지 생성
	unsigned char* paddingImage = (unsigned char*) calloc(sizeof(unsigned char), padImgWidth*padImgHeight);
	getPaddingImage(dprevFrm.decodedY, paddingImage, padImgWidth, padlen, width, height);

	unsigned char* Ychannel = (unsigned char*)calloc(width*height, sizeof(unsigned char));
	dcntFrm.decodedY  = (unsigned char*) malloc(sizeof(unsigned char)*width*height);

	int refx=0, refy=0, cntx=0, cnty=0;
	int cntindex=0, refindex=0;
	for(int nblck=0; nblck<totalblck; nblck++)
	{
		DBlockData &cntbd  = dcntFrm.dblocks[nblck];
		DBlockData &prevbd = dprevFrm.dblocks[nblck];
		cntx = (nblck%splitWidth)*blocksize1;
		cnty = (nblck/splitWidth)*blocksize1;
		refx = cntx - dcntFrm.dblocks[nblck].Reconstructedmv.x + padlen;
		refy = cnty - dcntFrm.dblocks[nblck].Reconstructedmv.y + padlen;

		for(int y=0; y<blocksize1; y++)
		{
			for(int x=0; x<blocksize1; x++)
			{
				cntindex=((y*width)+(cnty*width))+(cntx)+ x;
				refindex=((y*padImgWidth)+(refy*padImgWidth))+(refx)+ x;
				temp = paddingImage[refindex] + cntbd.interInverseErrblck16->block[y][x];
				temp = (temp>255) ? 255:temp;
				temp = (temp<0)   ? 0  :temp;
				Ychannel[cntindex] = temp;
			}
		}			
	}
	memcpy(dcntFrm.decodedY, Ychannel, sizeof(unsigned char)*width*height);
	free(Ychannel);
}
void getPaddingImage(unsigned char* src, unsigned char* dst, int padWidth, int padlen, int width, int height)
{

	for(int y=0; y<height; y++)
		for(int x=0; x<width; x++)
			dst[(y*padWidth+padlen*padWidth)+(x+padlen)] = src[y*width+x];

	/*checkResultY(dst, 384, 320);
	cout << "getpad" << endl;
	system("pause");*/

	// 윗줄
	for(int y=0; y<padlen; y++)
	{
		for(int x=0; x<width; x++)
		{
			dst[y*padWidth + (padlen+x)] = src[x];
			dst[(y+padlen+height-1)*padWidth + (x+padlen)] =src[(height-1)*width+x];
		}
	}

	// 아랫줄
	for(int y=0; y<height; y++)
	{
		for(int x=0; x<padlen; x++)
		{
			dst[((y+padlen)*padWidth) + x] = src[y*width];
			dst[((y+padlen)*padWidth) + x+(width+padlen-1)] = src[y*width+(width-1)];
		}
	}

	// 4 모서리
	for(int y=0; y<padlen; y++)
	{
		for(int x=0; x<padlen; x++)
		{
			dst[y*padWidth + x] = src[0];
			dst[y*padWidth + x+(width+padlen-1)]  = src[width-1];
			dst[(y+padlen+height-1)*padWidth + x] = src[(height-1)*width];
			dst[(y+padlen+height-1)*padWidth + x+(padlen+width-1)] = src[height*width-1];
		}
	}
}
void get16block(unsigned char* img, unsigned char *dst[16], int y0, int x0, int width, int blocksize)  // 블록의 크기를 16로 고정 ㅜㅜ
{
	// padimg size - width: 382 height: 320
	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			dst[y][x] = img[(y*width+y0*width)+x+x0];
		}
	}
}

/* check result function */
void checkResultFrames(DFrameData* frm, int width, int height, int nframe, int predtype, int chtype)
{
	FILE* output_fp;
	char CIF_path[256] = "..\\CIF(352x288)";

	char output_pred_name[256]; 
	if(predtype == INTRA)
		sprintf(output_pred_name, "check_test_intra");
	else if(predtype == INTER)
		sprintf(output_pred_name, "check_test_inter");

	char output_ch_name[256];
	if(chtype == SAVE_Y)
		sprintf(output_ch_name, "_y.yuv");
	else if(chtype == SAVE_YUV)
		sprintf(output_ch_name, "_yuv.yuv");


	char output_fname[256];
	sprintf(output_fname, "%s\\%s%s", CIF_path, output_pred_name, output_ch_name);

	output_fp = fopen(output_fname, "wb");
	if(output_fp==NULL)
	{
		cout << "fail to save yuv" << endl;
		return;
	}

	if(chtype==SAVE_Y)	// Y로만 된 영상 만들기
	{
		for(int i=0; i<nframe; i++)
		{
			fwrite(frm[i].decodedY, sizeof(unsigned char)*height*width, 1, output_fp);
		}
	}
	else if(chtype==SAVE_YUV)
	{
		for(int i=0; i<nframe; i++)
		{
			fwrite(frm[i].decodedY,  sizeof(unsigned char)*height*width, 1, output_fp);
			fwrite(frm[i].decodedCb,  sizeof(unsigned char)*(height/2)*(width/2), 1, output_fp);
			fwrite(frm[i].decodedCr,  sizeof(unsigned char)*(height/2)*(width/2), 1, output_fp);
		}
	}
	fclose(output_fp);

	for(int i=0; i<nframe; i++)
	{
		//free(frm[i].decodedY);
		free(frm[i].decodedCb);
		free(frm[i].decodedCr);
	}
}
