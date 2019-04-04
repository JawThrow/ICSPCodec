#include "ICSP_Codec_Encoder.h"


#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))


#define SIMD false


FILE* gfp;
char filename[256];

/* initiation function*/
int YCbCrLoad(IcspCodec &icC, char* fname, const int nframe,  const int width, const int height)
{
	icC.YCbCr.nframe = nframe;
	icC.YCbCr.width  = width;
	icC.YCbCr.height = height;

	/*char CIF_path[256] = "..\\CIF(352x288)";	
	char CIF_fname[256];*/
	
	char CIF_path[256] = "data";
	char CIF_fname[256];

	sprintf(CIF_fname, "%s\\%s", CIF_path, fname);

	FILE* input_fp;
	input_fp = fopen(CIF_fname, "rb");
	if(input_fp==NULL)
	{
		cout << "fail to load cif.yuv" << endl;
		return -1;
	}

	icC.YCbCr.Ys  = (unsigned char*) malloc(sizeof(unsigned char)*width*height*nframe);
	icC.YCbCr.Cbs = (unsigned char*) malloc(sizeof(unsigned char)*(width/2)*(height/2)*nframe);
	icC.YCbCr.Crs = (unsigned char*) malloc(sizeof(unsigned char)*(width/2)*(height/2)*nframe);

	if(icC.YCbCr.Ys == NULL || icC.YCbCr.Cbs == NULL || icC.YCbCr.Crs == NULL)
	{
		cout << "fail to malloc Ys, Cbs, Crs" << endl;
		return -1;
	}

	for(int i=0; i<nframe; i++)
	{
		fread(&icC.YCbCr.Ys[i*width*height], sizeof(unsigned char)*height*width, 1, input_fp);
		fread(&icC.YCbCr.Cbs[i*(width/2)*(height/2)], sizeof(unsigned char)*(width/2)*(height/2), 1, input_fp);
		fread(&icC.YCbCr.Crs[i*(width/2)*(height/2)], sizeof(unsigned char)*(width/2)*(height/2), 1, input_fp);
	}
	fclose(input_fp);

	return 0;
}
int splitFrames(IcspCodec &icC)
{
	int width  = icC.YCbCr.width;
	int height = icC.YCbCr.height;
	int nframe = icC.YCbCr.nframe;

	icC.frames = (FrameData *)malloc(sizeof(FrameData)*nframe);
	if(icC.frames == NULL)
	{
		cout << "fail to allocate memory to frames" << endl;
		return -1;
	}

	for(int numframe=0; numframe<nframe; numframe++)
	{
		FrameData &frm = icC.frames[numframe];
		frm.Y  = (unsigned char*) malloc(sizeof(unsigned char)*width*height);
		frm.Cb = (unsigned char*) malloc(sizeof(unsigned char)*(width/2)*(height/2));
		frm.Cr = (unsigned char*) malloc(sizeof(unsigned char)*(width/2)*(height/2));
		memcpy(frm.Y,  &icC.YCbCr.Ys[numframe*width*height], width*height);					  // Y 프레임 한장 저장; 나중에 뺄 수도 있음
		memcpy(frm.Cb, &icC.YCbCr.Cbs[numframe*(width/2)*(height/2)], (width/2)*(height/2));  // Cb 프레임 한장 저장;나중에 뺄 수도 있음
		memcpy(frm.Cr, &icC.YCbCr.Crs[numframe*(width/2)*(height/2)], (width/2)*(height/2));  // Cr 프레임 한장 저장;나중에 뺄 수도 있음	
		frm.numOfFrame = numframe;
	}

	return 0;
}
int splitBlocks(IcspCodec &icC, int blocksize1, int blocksize2)
{
	int width  = icC.YCbCr.width;
	int height = icC.YCbCr.height;
	int nframe = icC.YCbCr.nframe;	

	int splitWidth  = width  / blocksize1;	// 딱 떨어지는지 조건검사
	int splitHeight = height / blocksize1;  // 딱 떨어지는지 조건검사

	int totalblck = splitWidth * splitHeight;
	int nblck8 = icC.frames->nblocks8;
	if(width%blocksize1!=0 || height%blocksize1!=0)
	{
		cout << "improper value of blocksize1" << endl;
		return -1;
	}

	if(blocksize1%blocksize2!=0 || blocksize1%blocksize2!=0)
	{
		cout << "improper value of blocksize2" << endl;
		return -1;
	}

	//blocksize1xblocksize1을 blocksize2xblocksize2로 쪼갠 횟수
	int nblock2Ofblock1 = (int) (blocksize1*blocksize1) / (blocksize2*blocksize2);


	int CbCrSplitWidth  = (icC.YCbCr.width  / 2) / blocksize2;
	int CbCrSplitHeight = (icC.YCbCr.height / 2) / blocksize2;
	int CbCrWidth  = (icC.YCbCr.width   / 2);
	int CbCrHeight = (icC.YCbCr.height  / 2);
	int index = 0;
	int cntx=0, cnty=0;
	for(int numframe=0; numframe<nframe; numframe++)
	{
		/*frames 안에 있는 blocks 초기화*/
		icC.frames[numframe].blocks = (BlockData *) malloc(sizeof(BlockData)*splitWidth*splitHeight);	// 소멸자에서 반환해줘라
		if(icC.frames[numframe].blocks == NULL)
		{
			cout << "frame[" << numframe << "].blocks malloc fail..." << endl;
			return -1;
		}

		FrameData &frm = icC.frames[numframe];
		// Y frames
		for(int numblock=0; numblock<totalblck; numblock++)
		{
			BlockData &bd = icC.frames[numframe].blocks[numblock];			

			cntx=(numblock%splitWidth)*blocksize1;
			cnty=(numblock/splitWidth)*blocksize1;
			// 16x16 블록화	
			bd.originalblck16 = (Block16u*) malloc(sizeof(Block16u));
			for(int y=0; y<blocksize1; y++)
			{
				for(int x=0; x<blocksize1; x++)
				{	
					index = ((y*width)+(cnty*width))+(cntx)+x;
					bd.originalblck16->block[y][x] = frm.Y[index];
				}				
			}

			// 8x8 블록화
			bd.originalblck8 = (Block8u**)malloc(sizeof(Block8u*)*nblock2Ofblock1);
			for(int i=0; i<nblock2Ofblock1; i++)
				bd.originalblck8[i] = (Block8u*)malloc(sizeof(Block8u));

			for(int k=0; k<nblock2Ofblock1; k++)
			{
				for(int i=0; i<blocksize2; i++)
				{
					for(int j=0; j<blocksize2; j++)
					{
						if(k==0)
							bd.originalblck8[k]->block[i][j] = bd.originalblck16->block[i][j];
						if(k==1)
							bd.originalblck8[k]->block[i][j] = bd.originalblck16->block[i][j+blocksize2];
						if(k==2)
							bd.originalblck8[k]->block[i][j] = bd.originalblck16->block[i+blocksize2][j];
						if(k==3)
							bd.originalblck8[k]->block[i][j] = bd.originalblck16->block[i+blocksize2][j+blocksize2];
					}
				}				
			}	
			bd.numOfBlock16=numblock;
			bd.blocksize1 = blocksize1;
			bd.blocksize2 = blocksize2;
		}
		frm.nblocks16   = splitWidth*splitHeight;	// nblockxx 맴버가 FrameData에서 다른 구조체로 바뀔경우 위치 변경
		frm.nblocks8    = nblock2Ofblock1;
		frm.splitWidth  = splitWidth;
		frm.splitHeight = splitHeight;

		// Cb Cr frames
		icC.frames[numframe].Cbblocks = (CBlockData *) malloc(sizeof(CBlockData)*CbCrSplitWidth*CbCrSplitHeight);	// 소멸자에서 반환해줘라
		icC.frames[numframe].Crblocks = (CBlockData *) malloc(sizeof(CBlockData)*CbCrSplitWidth*CbCrSplitHeight);	

		for(int numOfblck=0; numOfblck<CbCrSplitWidth*CbCrSplitHeight; numOfblck++)
		{
			// 8x8 블록화
			CBlockData &cbbd = icC.frames[numframe].Cbblocks[numOfblck];
			CBlockData &crbd = icC.frames[numframe].Crblocks[numOfblck];

			cbbd.originalblck8 = (Block8u*)malloc(sizeof(Block8u));	// CDCT_block에서 free
			crbd.originalblck8 = (Block8u*)malloc(sizeof(Block8u)); // CDCT_block에서 free
			for(int y=0; y<blocksize2; y++)
			{
				for(int x=0; x<blocksize2; x++)
				{
					index = ((y*CbCrWidth)+(numOfblck/CbCrSplitWidth)*blocksize2*CbCrWidth)+(x+blocksize2*(numOfblck%CbCrSplitWidth));
					cbbd.originalblck8->block[y][x] = frm.Cb[index];
					crbd.originalblck8->block[y][x] = frm.Cr[index];
				}
			}
			cbbd.blocksize = blocksize2;
			crbd.blocksize = blocksize2;
		}
		frm.CbCrWidth = CbCrWidth;
		frm.CbCrHeight = CbCrHeight;
		frm.CbCrSplitWidth = CbCrSplitWidth;
		frm.CbCrSplitHeight = CbCrSplitHeight;
		frm.totalcbcrblck = CbCrWidth*CbCrHeight;
	}

	for(int i=0; i<icC.YCbCr.nframe; i++)
	{
		//free(icC.frames[i].Y);
		free(icC.frames[i].Cb);
		free(icC.frames[i].Cr);
	}

	return 0;
}

/* intra prediction function */
int allintraPrediction(FrameData* frames, int nframes, int QstepDC, int QstepAC)
{
#if SIMD
	gfp = fopen("Intra_MODE_Scalar.txt", "wt");
#else
	gfp = fopen("Intra_MODE_Scalar.txt", "wt");
#endif
	int totalblck = frames->nblocks16;
	int nblck8 = frames->nblocks8;
	int blocksize1 = frames->blocks->blocksize1;
	int blocksize2 = frames->blocks->blocksize2;
	int splitWidth = frames->splitWidth;
	int splitHeight = frames->splitHeight;

	for(int numOfFrm=0; numOfFrm<nframes; numOfFrm++)
	{
		FrameData& frm = frames[numOfFrm];
		for(int numOfblck16=0; numOfblck16<totalblck; numOfblck16++)
		{
			/*if (numOfblck16 == 95)
				cout << "!" << endl;*/
			BlockData& bd = frm.blocks[numOfblck16];
			CBlockData& cbbd = frm.Cbblocks[numOfblck16];
			CBlockData& crbd = frm.Crblocks[numOfblck16];

			/* 할당 구간 */
			bd.intraErrblck = (Block8i**)malloc(sizeof(Block8i*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraErrblck[i] = (Block8i*)malloc(sizeof(Block8i));
			
			bd.intraDCTblck = (Block8d**)malloc(sizeof(Block8d*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraDCTblck[i] = (Block8d*)malloc(sizeof(Block8d));

			bd.intraQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

			bd.intraInverseQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraInverseQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

			bd.intraInverseDCTblck = (Block8d**)malloc(sizeof(Block8d*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraInverseDCTblck[i] = (Block8d*)malloc(sizeof(Block8d));

			bd.intraRestructedblck8 = (Block8u**)malloc(sizeof(Block8u*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraRestructedblck8[i] = (Block8u*)malloc(sizeof(Block8u));

			cbbd.intraInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
			crbd.intraInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
			cbbd.intraInverseDCTblck  = (Block8d*)malloc(sizeof(Block8d));
			crbd.intraInverseDCTblck  = (Block8d*)malloc(sizeof(Block8d));
			/* 할당 구간 끝 */

			for(int numOfblck8=0; numOfblck8<nblck8; numOfblck8++)
			{
				DPCM_pix_block(frm, numOfblck16, numOfblck8, blocksize2, splitWidth);
				DCT_block(bd, numOfblck8, blocksize2, INTRA);
				DPCM_DC_block(frm, numOfblck16, numOfblck8, blocksize2, splitWidth, INTRA);
				Quantization_block(bd, numOfblck8, blocksize2, QstepDC, QstepAC, INTRA);
				
				reordering(bd, numOfblck8, INTRA);

				IQuantization_block(bd, numOfblck8, blocksize2, QstepDC, QstepAC, INTRA);
				IDPCM_DC_block(frm, numOfblck16, numOfblck8, blocksize2, splitWidth, INTRA);
				IDCT_block(bd, numOfblck8, blocksize2, INTRA);
				IDPCM_pix_block(frm, numOfblck16, numOfblck8, blocksize2, splitWidth);
			}			
			intraCbCr(frm, cbbd, crbd, blocksize2, numOfblck16, QstepDC, QstepAC);	// 5th parameter, numOfblck16, is numOfblck8 in CbCr
			mergeBlock(bd, blocksize2, INTRA);

			/* free 구간 */
			free(bd.originalblck8);
			free(bd.intraErrblck);
			free(bd.intraDCTblck);
			free(bd.intraQuanblck);			
			free(bd.intraInverseDCTblck);
			free(bd.originalblck16);
		}

		intraImgReconstruct(frm);
		//entropyCoding(frm, INTRA);

		/* free */
		for(int numOfblck16=0; numOfblck16<totalblck; numOfblck16++)
		{
			for(int numOfblck8=0; numOfblck8<nblck8; numOfblck8++)
				free(frm.blocks[numOfblck16].intraInverseQuanblck[numOfblck8]);
			free(frm.blocks[numOfblck16].intraInverseQuanblck);
			free(frm.Cbblocks[numOfblck16].intraInverseQuanblck);
			free(frm.Crblocks[numOfblck16].intraInverseQuanblck);
			free(frm.Cbblocks[numOfblck16].intraInverseDCTblck);
			free(frm.Crblocks[numOfblck16].intraInverseDCTblck);
		}
	}
	
	fclose(gfp);
	// restructedY는 checkResultFrames에서 사용하므로 free는 나중에 하자
	
	return 0;
}
void intraPrediction(FrameData& frm, int QstepDC, int QstepAC)
{
	// create Prediction blocks; mode 0: vertical 1: horizontal 2:DC	
	int totalblck = frm.nblocks16;
	int nblck8 = frm.nblocks8;
	int blocksize1 = frm.blocks->blocksize1;
	int blocksize2 = frm.blocks->blocksize2;
	int splitWidth = frm.splitWidth;
	int splitHeight = frm.splitHeight;
	
	//clock_t start = clock();
	for(int numOfblck16=0; numOfblck16<totalblck; numOfblck16++)
	{
		BlockData& bd = frm.blocks[numOfblck16];
		CBlockData& cbbd = frm.Cbblocks[numOfblck16];
		CBlockData& crbd = frm.Crblocks[numOfblck16];

		/* 할당 구간 */
		bd.intraErrblck = (Block8i**)malloc(sizeof(Block8i*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraErrblck[i] = (Block8i*)malloc(sizeof(Block8i));
		
		bd.intraDCTblck = (Block8d**)malloc(sizeof(Block8d*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraDCTblck[i] = (Block8d*)malloc(sizeof(Block8d));

		bd.intraQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

		bd.intraInverseQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraInverseQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

		bd.intraInverseDCTblck = (Block8d**)malloc(sizeof(Block8d*)*nblck8);
			for(int i=0; i<nblck8; i++)
				bd.intraInverseDCTblck[i] = (Block8d*)malloc(sizeof(Block8d));

		bd.intraRestructedblck8 = (Block8u**)malloc(sizeof(Block8u*)*nblck8);	// free는 이전 프레임을 갖고있어야 하므로 나중으로 미룸; 다음 interprediction연산에서 motioncompensation에 참조되므로 그때만 사용하고 free 시키면 될것
			for(int i=0; i<nblck8; i++)
				bd.intraRestructedblck8[i] = (Block8u*)malloc(sizeof(Block8u));
		
		cbbd.intraInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
		crbd.intraInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
		cbbd.intraInverseDCTblck = (Block8d*)malloc(sizeof(Block8d));
		crbd.intraInverseDCTblck = (Block8d*)malloc(sizeof(Block8d));
		/* 할당 구간 끝 */
		
		for(int numOfblck8=0; numOfblck8<nblck8; numOfblck8++)
		{
			DPCM_pix_block(frm, numOfblck16, numOfblck8, blocksize2, splitWidth);
			DCT_block(bd, numOfblck8, blocksize2, INTRA);
			DPCM_DC_block(frm, numOfblck16, numOfblck8, blocksize2, splitWidth, INTRA);
			Quantization_block(bd, numOfblck8, blocksize2, QstepDC, QstepAC, INTRA);

			reordering(bd, numOfblck8, INTRA);

			IQuantization_block(bd, numOfblck8, blocksize2, QstepDC, QstepAC, INTRA);
			IDPCM_DC_block(frm, numOfblck16, numOfblck8, blocksize2, splitWidth, INTRA);
			IDCT_block(bd, numOfblck8, blocksize2, INTRA);
			IDPCM_pix_block(frm, numOfblck16, numOfblck8, blocksize2, splitWidth);
		}		
		
		intraCbCr(frm, cbbd, crbd, blocksize2, numOfblck16, QstepDC, QstepAC); // numOfblck16 means numOfblck8 in CbCr Processing
		mergeBlock(bd, blocksize2, INTRA);		

		/* 해제 구간 */
		free(bd.originalblck8);
		free(bd.intraErrblck);
		free(bd.intraDCTblck);
		free(bd.intraQuanblck);
		free(bd.intraInverseDCTblck);
		free(bd.originalblck16);

	}
	//printf("%.5f\n", (float)(clock() - start) / CLOCKS_PER_SEC);
	intraImgReconstruct(frm);

	for(int numOfblck16=0; numOfblck16<totalblck; numOfblck16++)
	{
		for(int numOfblck8=0; numOfblck8<nblck8; numOfblck8++)
			free(frm.blocks[numOfblck16].intraInverseQuanblck[numOfblck8]);
		free(frm.blocks[numOfblck16].intraInverseQuanblck);
		free(frm.Cbblocks[numOfblck16].intraInverseQuanblck);
		free(frm.Crblocks[numOfblck16].intraInverseQuanblck);
		free(frm.Cbblocks[numOfblck16].intraInverseDCTblck);
		free(frm.Crblocks[numOfblck16].intraInverseDCTblck);
	}
}
int DPCM_pix_0(unsigned char upper[][8], unsigned char current[][8], int *err_temp[8], int blocksize) // vertical; 일단 첫번째 두번째 파라매터의 길이를 8로 static하게 고정
{
	int SAE=0;

#if SIMD
	
	int SAE_SIMD = 0;
	__m256i crntblck;
	__m256i predictionRow;
	__m256i subblck = _mm256_setzero_si256();
	__m256i absblck = _mm256_setzero_si256();
	__m256i tempblck[2];
	__m256i resblck[2];
	__mmMIXED _mixed;
	short errtemp[8][8];

	if (upper == NULL)
	{
		predictionRow = _mm256_set1_epi16(128);
	}
	else
	{
		__m256i tempPredLo = _mm256_cvtepu8_epi16(*(__m128i*)upper[blocksize - 1]);
		predictionRow = _mm256_set_m128i(*(__m128i*)&tempPredLo, *(__m128i*)&tempPredLo);
	}

	for (int y = 0; y < 2; y++)
	{
		_mixed.blck256 = _mm256_loadu_si256((__m256i*)current[y * 4]);
		crntblck = _mm256_cvtepu8_epi16(_mixed.blck128[0]);
		subblck = _mm256_sub_epi16(crntblck, predictionRow);
		absblck = _mm256_abs_epi16(subblck);
		_mm256_storeu_si256(tempblck, absblck);
		_mm256_storeu_si256((__m256i*)(errtemp + (4 * y)), subblck);

		crntblck = _mm256_cvtepu8_epi16(_mixed.blck128[1]);
		subblck = _mm256_sub_epi16(crntblck, predictionRow);
		absblck = _mm256_abs_epi16(subblck);
		_mm256_storeu_si256(tempblck +1, absblck);
		_mm256_storeu_si256((__m256i*)(errtemp + (4 * y + 2)), subblck);
		resblck[y] = _mm256_packs_epi16(tempblck[0], tempblck[1]);
	}

	__m256i temp;
	for (int i = 0; i < blocksize; i++)
	{
		temp = _mm256_cvtepi16_epi32(*(__m128i*)errtemp[i]);
		memcpy(err_temp[i], &temp, sizeof(int) * 8);
	}

	for (int i = 0; i < 32; i++)
		SAE_SIMD += (int)resblck[0].m256i_u8[i] + (int)resblck[1].m256i_u8[i];

	SAE = SAE_SIMD;
#else
	if(upper==NULL)
	{
		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				err_temp[y][x] = (int)current[y][x] - 128;
				SAE += abs(err_temp[y][x]);
			}
		}
	}
	else
	{
		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				err_temp[y][x] = (int)(current[y][x] - (int)upper[blocksize-1][x]);
				SAE += abs(err_temp[y][x]);
			}
		}
	}
#endif
	//cout << SAE << "\n";
	//fprintf(gfp, "%d\n", SAE);
	return SAE;
}
int DPCM_pix_1(unsigned char left[][8], unsigned char current[][8], int *err_temp[8], int blocksize) // horizontal; 일단 첫번째 두번째 파라매터의 길이를 8로 static하게 고정
{
	int SAE = 0;
#if SIMD
	int SAE_SIMD = 0;
	
	__m256i predictionRow;
	__m256i currentblck;
	__m256i subblck = _mm256_setzero_si256();
	__m256i absblck = _mm256_setzero_si256();
	__m256i tempblck[2];
	__m256i resblck[2];	
	__mmMIXED _mixed;
	__mmMIXED PredMixed[4];
	short errtemp[8][8] = { 0, };
	if (left == NULL)
	{
		for (int i = 0; i < blocksize; i++)
			PredMixed[i / 2].blck128[i % 2] = _mm_set1_epi16(128);
	}
	else
	{
		for (int i = 0; i < blocksize; i++)
		{
			PredMixed[i / 2].blck128[i % 2] = _mm_set1_epi16((short)left[i][blocksize - 1]);
		}
	}

	for (int y = 0; y < 2; y++)
	{
		_mixed.blck256 = _mm256_loadu_si256((__m256i*)current[y * 4]);
		predictionRow = _mm256_set_m128i(PredMixed[2 * y].blck128[1], PredMixed[2 * y].blck128[0]);
		currentblck = _mm256_cvtepu8_epi16(_mixed.blck128[0]);		
		subblck = _mm256_sub_epi16(currentblck, predictionRow);
		absblck = _mm256_abs_epi16(subblck);
		_mm256_storeu_si256(tempblck, absblck);
		_mm256_storeu_si256((__m256i*)(errtemp + (4 * y)), subblck);

		predictionRow = _mm256_set_m128i(PredMixed[2 * y + 1].blck128[1], PredMixed[2 * y + 1].blck128[0]);
		currentblck = _mm256_cvtepu8_epi16(_mixed.blck128[1]);
		subblck = _mm256_sub_epi16(currentblck, predictionRow);
		absblck = _mm256_abs_epi16(subblck);
		_mm256_storeu_si256(tempblck + 1, absblck);
		_mm256_storeu_si256((__m256i*)(errtemp + (4 * y + 2)), subblck);
		resblck[y] = _mm256_packs_epi16(tempblck[0], tempblck[1]);
	}

	
	__m256i temp;
	for (int i = 0; i < blocksize; i++)
	{
		temp = _mm256_cvtepi16_epi32(*(__m128i*)errtemp[i]);
		memcpy(err_temp[i], &temp, sizeof(int)*blocksize);				

	}



	for (int i = 0; i < 32; i++)
		SAE_SIMD += (int)resblck[0].m256i_u8[i] + (int)resblck[1].m256i_u8[i];

	
	
	SAE = SAE_SIMD;
#else
	if (left == NULL)
	{
		for (int y = 0; y<blocksize; y++)
		{
			for (int x = 0; x<blocksize; x++)
			{
				err_temp[y][x] = (int)current[y][x] - 128;
				SAE += abs(err_temp[y][x]);
			}
		}
	}
	else
	{
		for (int y = 0; y<blocksize; y++)
		{
			for (int x = 0; x<blocksize; x++)
			{
				err_temp[y][x] = (int)current[y][x] - (int)left[y][blocksize - 1];
				SAE += abs(err_temp[y][x]);
			}
		}
	}
#endif
	//fprintf(gfp, "%d\n", SAE);
	return SAE;
}
int DPCM_pix_2(unsigned char left[][8], unsigned char upper[][8], unsigned char current[][8], int *err_temp[8], int blocksize) // DC; 일단 첫번째 두번째 파라매터의 길이를 8로 static하게 고정
{
	double predValLeft  = 0;
	double predValUpper = 0;
	double predVal      = 0;
	int SAE = 0;

	if (left == NULL)
	{
		predValLeft = 128 * 8;
	}
	else
	{
		for (int i = 0; i<blocksize; i++)
			predValLeft += left[i][blocksize - 1];
	}

	if (upper == NULL)
	{
		predValUpper = 128 * 8;
	}
	else
	{
		for (int i = 0; i<blocksize; i++)
			predValUpper += upper[blocksize - 1][i];
	}

	predVal = (predValLeft + predValUpper) / (double)(blocksize + blocksize);

#if SIMD
	int SAE_SIMD = 0;
	__m256i predictionRow;
	__m256i crntblck;
	__m256i subblck = _mm256_setzero_si256();
	__m256i absblck = _mm256_setzero_si256();
	__m256i tempblck[2];
	__m256i resblck[2];
	__mmMIXED _mixed;
	short errtemp[8][8];

	predictionRow = _mm256_set1_epi16(predVal);
	
	for (int y = 0; y < 2; y++)
	{
		_mixed.blck256 = _mm256_loadu_si256((__m256i*)current[y * 4]);
		crntblck = _mm256_cvtepu8_epi16(_mixed.blck128[0]);
		subblck = _mm256_sub_epi16(crntblck, predictionRow);
		absblck = _mm256_abs_epi16(subblck);
		_mm256_storeu_si256(tempblck, absblck);
		_mm256_storeu_si256((__m256i*)(errtemp + (4 * y)), subblck);

		crntblck = _mm256_cvtepu8_epi16(_mixed.blck128[1]);
		subblck = _mm256_sub_epi16(crntblck, predictionRow);
		absblck = _mm256_abs_epi16(subblck);
		_mm256_storeu_si256(tempblck + 1, absblck);
		_mm256_storeu_si256((__m256i*)(errtemp + (4 * y + 2)), subblck);
		resblck[y] = _mm256_packs_epi16(tempblck[0], tempblck[1]);
	}


	__m256i temp;
	for (int i = 0; i < blocksize; i++)
	{
		temp = _mm256_cvtepi16_epi32(*(__m128i*)errtemp[i]);
		memcpy(err_temp[i], &temp, sizeof(int)*blocksize);
	}

	for (int i = 0; i < 32; i++)
		SAE_SIMD += (int)resblck[0].m256i_u8[i] + (int)resblck[1].m256i_u8[i];


	SAE = SAE_SIMD;
#else	
	for (int y = 0; y<blocksize; y++)
	{
		for (int x = 0; x<blocksize; x++)
		{
			err_temp[y][x] = (int)current[y][x] - predVal;
			SAE += abs(err_temp[y][x]);
		}
	}
#endif
	//fprintf(gfp, "%d\n", SAE);
	return SAE;
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
void DPCM_pix_block(FrameData &frm, int numOfblck16, int numOfblck8, int blocksize, int splitWidth)
{
	int **temp0;
	int **temp1;
	int **temp2;
	temp0 = (int**)malloc(sizeof(int*)*blocksize);
	temp1 = (int**)malloc(sizeof(int*)*blocksize);
	temp2 = (int**)malloc(sizeof(int*)*blocksize);

	if (temp0 == NULL || temp1 == NULL || temp2 == NULL)
	{
		printf("Memory allocation error!\n");		
	}

	for(int i=0; i<blocksize; i++)
	{
		temp0[i] = (int*)calloc(blocksize, sizeof(int));
		temp1[i] = (int*)calloc(blocksize, sizeof(int));
		temp2[i] = (int*)calloc(blocksize, sizeof(int));
	}

	int SAE0=0, SAE1=0, SAE2=0;
	int m = 0; // SAE중 최소값이 저장될 변수
	int numOfCurrentBlck=0;
	int predmode1=0, predmode2=0, predmode3=0, cpredmode=0;
	int median=0;
	
	BlockData& bd = frm.blocks[numOfblck16];
	bd.intraPredMode[numOfblck8] = 0;	// 초기화
	if(numOfblck16 == 0) // 16x16블록의 위치가 첫 번째이다
	{
		switch(numOfblck8)
		{
		case 0:
			numOfCurrentBlck=0;
			DPCM_pix_2(NULL, NULL, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			for(int i=0; i<blocksize; i++)
				memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			bd.DPCMmodePred[numOfCurrentBlck] = 2;
			bd.MPMFlag[numOfCurrentBlck] = 0;
			bd.intraPredMode[numOfCurrentBlck] = 0;
			break;
		case 1:
			numOfCurrentBlck=1;
			SAE1 = DPCM_pix_1(bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(bd.intraRestructedblck8[0]->block, NULL, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			if (SAE2>SAE1) 
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else // 같을 때는 ???
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=bd.DPCMmodePred[0];			
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==predmode1)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(predmode1==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)? 0:1;
				else if(predmode1==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
			}
			break;
		case 2:
			numOfCurrentBlck=2;
			SAE0 = DPCM_pix_0(bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE2 = DPCM_pix_2(NULL, bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			if(SAE2>SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=bd.DPCMmodePred[0];
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==predmode1)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(predmode1==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)? 0:1;
				else if(predmode1==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
			}
			break;
		case 3:
			numOfCurrentBlck=3;
			SAE0 = DPCM_pix_0(bd.intraRestructedblck8[1]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE1 = DPCM_pix_1(bd.intraRestructedblck8[2]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(bd.intraRestructedblck8[2]->block, bd.intraRestructedblck8[1]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			m = min(SAE0, SAE1); m = min(m, SAE2);
			if(m==SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else if(m==SAE1)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=bd.DPCMmodePred[0];
			predmode2=bd.DPCMmodePred[1];
			predmode3=bd.DPCMmodePred[2];

			// median
			if( (predmode1>predmode2) && (predmode1>predmode3) )	 median=(predmode2>predmode3) ? predmode2:predmode3;
			else if((predmode2>predmode1) && (predmode2>predmode3))	 median=(predmode1>predmode3) ? predmode1:predmode3;
			else		   											 median=(predmode1>predmode2) ? predmode1:predmode2;
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==median)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				//bd.intraPredMode[numOfCurrentBlck] = (median>cpredmode)? 0:1;
				if(median==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)? 0:1;
				else if(median==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (median>cpredmode)? 0:1;
			}
			break;
		}
	}
	else if(numOfblck16/splitWidth == 0) // 16x16의 위치가 첫 행이다.
	{
		switch(numOfblck8)
		{
		case 0:
			numOfCurrentBlck=0;
			SAE1 = DPCM_pix_1(frm.blocks[numOfblck16-1].intraRestructedblck8[1]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(frm.blocks[numOfblck16-1].intraRestructedblck8[1]->block, NULL, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			if (SAE2>SAE1) 
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else // 같을 때는 ???
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode = bd.DPCMmodePred[numOfCurrentBlck];
			predmode1 = frm.blocks[numOfblck16-1].DPCMmodePred[1];
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==predmode1)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(predmode1==0) 
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)? 0:1;
				else if(predmode1==2) 
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
				else 
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
			}
			break;
		case 1:
			numOfCurrentBlck=1;
			SAE1 = DPCM_pix_1(bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(bd.intraRestructedblck8[0]->block, NULL, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			if (SAE2>SAE1) 
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else // 같을 때는 ???
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode = bd.DPCMmodePred[numOfCurrentBlck];
			predmode1 = bd.DPCMmodePred[0];
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==predmode1)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(predmode1==0) 
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)? 0:1;
				else if(predmode1==2) 
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
				else 
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
			}
			break;
		case 2:
			numOfCurrentBlck=2;
			SAE0 = DPCM_pix_0(bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE1 = DPCM_pix_1(frm.blocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(frm.blocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			m = min(SAE0, SAE1); m = min(m, SAE2);
			if(m==SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else if(m==SAE1)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=frm.blocks[numOfblck16-1].DPCMmodePred[3];
			predmode2=frm.blocks[numOfblck16-1].DPCMmodePred[1];
			predmode3=bd.DPCMmodePred[0];
			if( (predmode1>predmode2) && (predmode1>predmode3) )	 median=(predmode2>predmode3) ? predmode2:predmode3;
			else if((predmode2>predmode1) && (predmode2>predmode3))	 median=(predmode1>predmode3) ? predmode1:predmode3;
			else		   											 median=(predmode1>predmode2) ? predmode1:predmode2;
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==median)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(median==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)? 0:1;
				else if(median==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (median>cpredmode)? 0:1;
			}
			break;
		case 3:
			numOfCurrentBlck=3;
			SAE0 = DPCM_pix_0(bd.intraRestructedblck8[1]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE1 = DPCM_pix_1(bd.intraRestructedblck8[2]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(bd.intraRestructedblck8[2]->block, bd.intraRestructedblck8[1]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			m = min(SAE0, SAE1); m = min(m, SAE2);
			if(m==SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else if(m==SAE1)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=bd.DPCMmodePred[0];
			predmode2=bd.DPCMmodePred[1];
			predmode3=bd.DPCMmodePred[2];
			if( (predmode1>predmode2) && (predmode1>predmode3) )	 median=(predmode2>predmode3) ? predmode2:predmode3;
			else if((predmode2>predmode1) && (predmode2>predmode3))	 median=(predmode1>predmode3) ? predmode1:predmode3;
			else		   											 median=(predmode1>predmode2) ? predmode1:predmode2;
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==median)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(median==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)?0:1;
				else if(median==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)?0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (median>cpredmode)? 0:1;
			}			
			break;
		}
	}
	else if(numOfblck16%splitWidth == 0) // 16x16 블록의 위치가 첫 열이다.
	{
		switch(numOfblck8)
		{
		case 0:
			numOfCurrentBlck=0;
			SAE0 = DPCM_pix_0(frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE2 = DPCM_pix_2(NULL, frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			if(SAE2>SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode = bd.DPCMmodePred[numOfCurrentBlck];
			predmode1 = frm.blocks[numOfblck16-splitWidth].DPCMmodePred[2];
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==predmode1)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(predmode1==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)? 0:1;
				else if(predmode1==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
			}
			break;
		case 1:
			numOfCurrentBlck=1;
			SAE0 = DPCM_pix_0(frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE1 = DPCM_pix_1(bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(bd.intraRestructedblck8[0]->block, frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			m = min(SAE0, SAE1); m = min(m, SAE2);
			if(m==SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else if(m==SAE1)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=bd.DPCMmodePred[0];
			predmode2=frm.blocks[numOfblck16-splitWidth].DPCMmodePred[2];
			predmode3=frm.blocks[numOfblck16-splitWidth].DPCMmodePred[3];
			if( (predmode1>predmode2) && (predmode1>predmode3) )	 median=(predmode2>predmode3) ? predmode2:predmode3;
			else if((predmode2>predmode1) && (predmode2>predmode3))	 median=(predmode1>predmode3) ? predmode1:predmode3;
			else		   											 median=(predmode1>predmode2) ? predmode1:predmode2;
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==median)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(median==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)? 0:1;
				else if(median==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (median>cpredmode)? 0:1;
			}
			break;
		case 2:
			numOfCurrentBlck=2;
			SAE0 = DPCM_pix_0(bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE2 = DPCM_pix_2(NULL, bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			if(SAE2>SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode = bd.DPCMmodePred[numOfCurrentBlck];
			predmode1 = bd.DPCMmodePred[0];
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==predmode1)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(predmode1==0) 
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)? 0:1;
				else if(predmode1==2) 
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
				else 
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)? 0:1;
			}
			break;
		case 3:
			numOfCurrentBlck=3;
			SAE0 = DPCM_pix_0(bd.intraRestructedblck8[1]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE1 = DPCM_pix_1(bd.intraRestructedblck8[2]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(bd.intraRestructedblck8[2]->block, bd.intraRestructedblck8[1]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			m = min(SAE0, SAE1); m = min(m, SAE2);
			if(m==SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else if(m==SAE1)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=bd.DPCMmodePred[0];
			predmode2=bd.DPCMmodePred[1];
			predmode3=bd.DPCMmodePred[2];
			if( (predmode1>predmode2) && (predmode1>predmode3) )	 median=(predmode2>predmode3) ? predmode2:predmode3;
			else if((predmode2>predmode1) && (predmode2>predmode3))	 median=(predmode1>predmode3) ? predmode1:predmode3;
			else		   											 median=(predmode1>predmode2) ? predmode1:predmode2;
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==median)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(median==0)
					bd.intraPredMode[numOfCurrentBlck] = (bd.DPCMmodePred[numOfCurrentBlck]==1)?0:1;
				else if(median==2)
					bd.intraPredMode[numOfCurrentBlck] = (bd.DPCMmodePred[numOfCurrentBlck]==0)?0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (median>cpredmode)? 0:1;
			}
			break;
		}
	}
	else // 16x16 블록이 그 외의 위치에 있다.
	{
		switch(numOfblck8)
		{
		case 0:
			numOfCurrentBlck=0;
			SAE0 = DPCM_pix_0(frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE1 = DPCM_pix_1(frm.blocks[numOfblck16-1].intraRestructedblck8[1]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(frm.blocks[numOfblck16-1].intraRestructedblck8[1]->block, frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			m = min(SAE0, SAE1); m = min(m, SAE2);
			if(m==SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else if(m==SAE1)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=frm.blocks[numOfblck16-1].DPCMmodePred[1];
			predmode2=frm.blocks[numOfblck16-splitWidth-1].DPCMmodePred[3];
			predmode3=frm.blocks[numOfblck16-splitWidth].DPCMmodePred[2];
			if( (predmode1>predmode2) && (predmode1>predmode3) )	 median=(predmode2>predmode3) ? predmode2:predmode3;
			else if((predmode2>predmode1) && (predmode2>predmode3))	 median=(predmode1>predmode3) ? predmode1:predmode3;
			else		   											 median=(predmode1>predmode2) ? predmode1:predmode2;
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==median)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(median==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)?0:1;
				else if(median==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)?0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (median>cpredmode)? 0:1;
			}
			break;
		case 1:
			numOfCurrentBlck=1;
			SAE0 = DPCM_pix_0(frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE1 = DPCM_pix_1(bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(bd.intraRestructedblck8[0]->block, frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			m = min(SAE0, SAE1); m = min(m, SAE2);
			if(m==SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else if(m==SAE1)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=bd.DPCMmodePred[0];
			predmode2=frm.blocks[numOfblck16-splitWidth].DPCMmodePred[2];
			predmode3=frm.blocks[numOfblck16-splitWidth].DPCMmodePred[3];
			if( (predmode1>predmode2) && (predmode1>predmode3) )	 median=(predmode2>predmode3) ? predmode2:predmode3;
			else if((predmode2>predmode1) && (predmode2>predmode3))	 median=(predmode1>predmode3) ? predmode1:predmode3;
			else		   											 median=(predmode1>predmode2) ? predmode1:predmode2;
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==median)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(median==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)?0:1;
				else if(median==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)?0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (median>cpredmode)? 0:1;
			}
			break;
		case 2:
			numOfCurrentBlck=2;
			SAE0 = DPCM_pix_0(bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE1 = DPCM_pix_1(frm.blocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(frm.blocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.intraRestructedblck8[0]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			m = min(SAE0, SAE1); m = min(m, SAE2);
			if(m==SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else if(m==SAE1)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=frm.blocks[numOfblck16-1].DPCMmodePred[3];
			predmode2=frm.blocks[numOfblck16-1].DPCMmodePred[1];
			predmode3=bd.DPCMmodePred[0];
			if( (predmode1>predmode2) && (predmode1>predmode3) )	 median=(predmode2>predmode3) ? predmode2:predmode3;
			else if((predmode2>predmode1) && (predmode2>predmode3))	 median=(predmode1>predmode3) ? predmode1:predmode3;
			else		   											 median=(predmode1>predmode2) ? predmode1:predmode2;
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==median)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(median==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)?0:1;
				else if(median==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)?0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (median>cpredmode)? 0:1;
			}
			break;
		case 3:
			numOfCurrentBlck=3;
			SAE0 = DPCM_pix_0(bd.intraRestructedblck8[1]->block, bd.originalblck8[numOfCurrentBlck]->block, temp0, blocksize);
			SAE1 = DPCM_pix_1(bd.intraRestructedblck8[2]->block, bd.originalblck8[numOfCurrentBlck]->block, temp1, blocksize);
			SAE2 = DPCM_pix_2(bd.intraRestructedblck8[2]->block, bd.intraRestructedblck8[1]->block, bd.originalblck8[numOfCurrentBlck]->block, temp2, blocksize);
			m = min(SAE0, SAE1); m = min(m, SAE2);
			if(m==SAE0)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 0;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp0[i], sizeof(int)*blocksize);
			}
			else if(m==SAE1)
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 1;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp1[i], sizeof(int)*blocksize);
			}
			else
			{
				bd.DPCMmodePred[numOfCurrentBlck] = 2;
				for(int i=0; i<blocksize; i++)
					memcpy(bd.intraErrblck[numOfCurrentBlck]->block[i], temp2[i], sizeof(int)*blocksize);
			}

			cpredmode=bd.DPCMmodePred[numOfCurrentBlck];
			predmode1=bd.DPCMmodePred[0];
			predmode2=bd.DPCMmodePred[1];
			predmode3=bd.DPCMmodePred[2];
			if( (predmode1>predmode2) && (predmode1>predmode3) )	 median=(predmode2>predmode3) ? predmode2:predmode3;
			else if((predmode2>predmode1) && (predmode2>predmode3))	 median=(predmode1>predmode3) ? predmode1:predmode3;
			else		   											 median=(predmode1>predmode2) ? predmode1:predmode2;
			bd.MPMFlag[numOfCurrentBlck] = (cpredmode==median)? 1:0;
			if(!bd.MPMFlag[numOfCurrentBlck])
			{
				if(median==0)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==1)?0:1;
				else if(median==2)
					bd.intraPredMode[numOfCurrentBlck] = (cpredmode==0)?0:1;
				else
					bd.intraPredMode[numOfCurrentBlck] = (median>cpredmode)? 0:1;
			}
			break;
		}
	}


	//fprintf(gfp, "%d\n", bd.DPCMmodePred[numOfCurrentBlck]);

	for(int i=0; i<blocksize; i++)
	{
		free(temp0[i]);
		free(temp1[i]);
		free(temp2[i]);
	}
	free(temp0);
	free(temp1);
	free(temp2);
	
	// restructedblck8 is needed to free here
	free(bd.originalblck8[numOfCurrentBlck]);
}
void IDPCM_pix_block(FrameData &frm, int numOfblck16, int numOfblck8, int blocksize, int splitWidth)
{
	int numOfCurrentBlck = 0;
	int predMode = 0;
	int modeblck0=0, modeblck1=0, modeblck2=0;
	int modetemp0=0, modetemp1=0, modetemp2=0;
	int median   = 0;
	BlockData &bd = frm.blocks[numOfblck16];
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
			modetemp0=frm.blocks[numOfblck16-1].IDPCMmode[1];
			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = frm.blocks[numOfblck16-1].IDPCMmode[1];
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
			else if(predMode==1) IDPCM_pix_1(frm.blocks[numOfblck16-1].intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(frm.blocks[numOfblck16-1].intraRestructedblck8[1]->block, NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);

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
			modetemp1 = frm.blocks[numOfblck16-1].IDPCMmode[1];
			modetemp2 = frm.blocks[numOfblck16-1].IDPCMmode[3];
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
			else if(predMode==1) IDPCM_pix_1(frm.blocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(frm.blocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);

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
			modetemp0 = frm.blocks[numOfblck16-splitWidth].IDPCMmode[2];
			if(bd.MPMFlag[numOfCurrentBlck]==1)
				predMode = frm.blocks[numOfblck16-splitWidth].IDPCMmode[2];
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
			if(predMode==0)		 IDPCM_pix_0(frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(NULL, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(NULL, frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 1:
			numOfCurrentBlck=1;
			modetemp0 = bd.IDPCMmode[0];
			modetemp1 = frm.blocks[numOfblck16-splitWidth].IDPCMmode[2];
			modetemp2 = frm.blocks[numOfblck16-splitWidth].IDPCMmode[3];
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
			if(predMode==0)		 IDPCM_pix_0(frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(bd.intraRestructedblck8[0]->block, frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
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
			modetemp0 = frm.blocks[numOfblck16-1].IDPCMmode[1];
			modetemp1 = frm.blocks[numOfblck16-splitWidth].IDPCMmode[2];
			modetemp2 = frm.blocks[numOfblck16-splitWidth-1].IDPCMmode[3];
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
			if(predMode==0)		 IDPCM_pix_0(frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(frm.blocks[numOfblck16-1].intraRestructedblck8[1]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(frm.blocks[numOfblck16-1].intraRestructedblck8[1]->block, frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[2]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 1:
			numOfCurrentBlck=1;
			modetemp0 = bd.IDPCMmode[0];
			modetemp1 = frm.blocks[numOfblck16-splitWidth].IDPCMmode[2];
			modetemp2 = frm.blocks[numOfblck16-splitWidth].IDPCMmode[3];
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
			if(predMode==0)		 IDPCM_pix_0(frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==1) IDPCM_pix_1(bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(bd.intraRestructedblck8[0]->block, frm.blocks[numOfblck16-splitWidth].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			break;
		case 2:
			numOfCurrentBlck=2;
			modetemp0 = bd.IDPCMmode[0];
			modetemp1 = frm.blocks[numOfblck16-1].IDPCMmode[1];
			modetemp2 = frm.blocks[numOfblck16-1].IDPCMmode[3];
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
			else if(predMode==1) IDPCM_pix_1(frm.blocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
			else if(predMode==2) IDPCM_pix_2(frm.blocks[numOfblck16-1].intraRestructedblck8[3]->block, bd.intraRestructedblck8[0]->block, bd.intraInverseDCTblck[numOfCurrentBlck]->block, bd.intraRestructedblck8[numOfCurrentBlck]->block, blocksize);
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
void intraCbCr(FrameData& frm, CBlockData &cbbd, CBlockData &crbd, int blocksize, int numOfblck8, int QstepDC, int QstepAC)
{
	// Cb
	cbbd.intraDCTblck = (Block8d*)malloc(sizeof(Block8d));
	cbbd.intraQuanblck = (Block8i*)malloc(sizeof(Block8i));

	CDCT_block(cbbd, blocksize, INTRA);
	CDPCM_DC_block(frm, cbbd, numOfblck8, CB, INTRA);
	CQuantization_block(cbbd, blocksize, QstepDC, QstepAC, INTRA);
	Creordering(cbbd, INTRA);
	CIQuantization_block(cbbd, blocksize, QstepDC, QstepAC, INTRA);
	CIDPCM_DC_block(frm, cbbd, numOfblck8, CB, INTRA);
	CIDCT_block(cbbd, blocksize, INTRA);	


	// Cr
	crbd.intraDCTblck = (Block8d*)malloc(sizeof(Block8d));
	crbd.intraQuanblck = (Block8i*)malloc(sizeof(Block8i));

	CDCT_block(crbd, blocksize, INTRA);
	CDPCM_DC_block(frm, crbd, numOfblck8, CR, INTRA);
	CQuantization_block(crbd, blocksize, QstepDC, QstepAC, INTRA);
	Creordering(crbd, INTRA);
	CIQuantization_block(crbd, blocksize, QstepDC, QstepAC, INTRA);
	CIDPCM_DC_block(frm, crbd, numOfblck8, CR, INTRA);
	CIDCT_block(crbd, blocksize, INTRA);

}
void intraImgReconstruct(FrameData &frm)
{
	int blocksize1  = frm.blocks->blocksize1;
	int blocksize2  = frm.blocks->blocksize2;
	int totalblck   = frm.nblocks16;
	int nblck8      = frm.nblocks8;
	int splitWidth  = frm.splitWidth;
	int splitHeight = frm.splitHeight;
	int CbCrSplitWidth  = frm.CbCrSplitWidth;
	int CbCrSplitHeight = frm.CbCrSplitHeight;
	int CbCrWidth  = frm.CbCrWidth;
	int CbCrHeight = frm.CbCrHeight;
	int width = splitWidth*blocksize1;
	int height = splitHeight*blocksize1;

	unsigned char* Ychannel = (unsigned char*)calloc(width*height, sizeof(unsigned char));
	unsigned char* Cbchannel = (unsigned char *) calloc((width/2) * (height/2), sizeof(unsigned char));
	unsigned char* Crchannel = (unsigned char *) calloc((width/2) * (height/2), sizeof(unsigned char));

	frm.reconstructedY  = (unsigned char*) malloc(sizeof(unsigned char)*width*height);				//checkResultFrames함수에서 반환
	frm.reconstructedCb = (unsigned char*) malloc(sizeof(unsigned char)*(width/2)*(height/2));		//checkResultFrames에서 반환
	frm.reconstructedCr = (unsigned char*) malloc(sizeof(unsigned char)*(width/2)*(height/2));		//checkResultFrames에서 반환

	int temp = 0;
	int tempCb = 0;
	int tempCr = 0;
	int nblck=0;	


	nblck=0;
	for(int y_interval=0; y_interval<splitHeight; y_interval++)
	{
		for(int x_interval=0; x_interval<splitWidth; x_interval++)
		{			
			BlockData &bd = frm.blocks[nblck]; // nblck++

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
	memcpy(frm.reconstructedY, Ychannel, sizeof(unsigned char)*width*height);

	nblck=0;
	for(int y_interval=0; y_interval<CbCrSplitHeight; y_interval++)
	{
		for(int x_interval=0; x_interval<CbCrSplitWidth; x_interval++)
		{			
			CBlockData &cbbd = frm.Cbblocks[nblck]; // nblck++
			CBlockData &crbd = frm.Crblocks[nblck];
			int index=0;
			for(int y=0; y<blocksize2; y++)
			{
				for(int x=0; x<blocksize2; x++)
				{							
					tempCb = (cbbd.intraInverseDCTblck->block[y][x]>255) ? 255:cbbd.intraInverseDCTblck->block[y][x];							
					tempCb = (tempCb<0) ? 0 :tempCb;
					index = (y_interval*blocksize2*CbCrWidth) + (y*CbCrWidth) + (x_interval*blocksize2)+x;
					Cbchannel[index] = tempCb;

					tempCr = (crbd.intraInverseDCTblck->block[y][x]>255) ? 255:crbd.intraInverseDCTblck->block[y][x];
					tempCr = (tempCr<0) ? 0:tempCr;
					Crchannel[index] = tempCr;
				}
			}
			nblck++;
		}		
	}	
	memcpy(frm.reconstructedCb, Cbchannel, sizeof(unsigned char)*(width/2)*(height/2));
	memcpy(frm.reconstructedCr, Crchannel, sizeof(unsigned char)*(width/2)*(height/2));

	free(Ychannel);
	free(Cbchannel);
	free(Crchannel);
}

/* inter prediction function */
int interPrediction(FrameData& cntFrm, FrameData& prevFrm, int QstepDC, int QstepAC)
{
#if SIMD
	gfp = fopen("Inter_getSAD_vector.txt", "wt");
#else
	gfp = fopen("Inter_getSAD_scalar.txt", "wt");
#endif
	int totalblck = cntFrm.nblocks16;
	int nblock8 = cntFrm.nblocks8;
	int blocksize1 = cntFrm.blocks->blocksize1;
	int blocksize2 = cntFrm.blocks->blocksize2;
	int splitWidth = cntFrm.splitWidth;
		
	motionEstimation(cntFrm, prevFrm);

	for(int nblck=0; nblck<totalblck; nblck++)
	{
		cntFrm.blocks[nblck].interErrblck16 = (Block16i*)malloc(sizeof(Block16i));
		cntFrm.blocks[nblck].interErrblck8 = (Block8i**)malloc(sizeof(Block8i*)*nblock8); // free는 DCT_block에서 해줌
		for(int i=0; i<nblock8; i++)
			cntFrm.blocks[nblck].interErrblck8[i] = (Block8i*)malloc(sizeof(Block8i));
	}
	
	motionCompensation(cntFrm, prevFrm);
	
	for(int nblck=0; nblck<totalblck; nblck++)
		free(cntFrm.blocks[nblck].interErrblck16);


	for(int nblck=0; nblck<totalblck; nblck++)
	{
		mvPrediction(cntFrm, nblck);

		/* 할당~ */
		cntFrm.blocks[nblck].interDCTblck = (Block8d**)malloc(sizeof(Block8d*)*nblock8);
		for(int i=0; i<nblock8; i++)
			cntFrm.blocks[nblck].interDCTblck[i] = (Block8d*)malloc(sizeof(Block8d));
				
		cntFrm.blocks[nblck].interQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblock8);
		for(int i=0; i<nblock8; i++)
			cntFrm.blocks[nblck].interQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

		cntFrm.blocks[nblck].interInverseQuanblck = (Block8i**)malloc(sizeof(Block8i*)*nblock8);
		for(int i=0; i<nblock8; i++)
			cntFrm.blocks[nblck].interInverseQuanblck[i] = (Block8i*)malloc(sizeof(Block8i));

		cntFrm.blocks[nblck].interInverseDCTblck = (Block8d**)malloc(sizeof(Block8d*)*nblock8);
		for(int i=0; i<nblock8; i++)
			cntFrm.blocks[nblck].interInverseDCTblck[i] = (Block8d*)malloc(sizeof(Block8d));

		cntFrm.blocks[nblck].interInverseErrblck16 = (Block16i*)malloc(sizeof(Block16i));
		/* 할당 끝*/

		for(int numOfblck8=0; numOfblck8<nblock8; numOfblck8++)
		{
			DCT_block(cntFrm.blocks[nblck], numOfblck8, blocksize2, INTER);	
			DPCM_DC_block(cntFrm, nblck, numOfblck8, blocksize2, splitWidth, INTER);
			Quantization_block(cntFrm.blocks[nblck], numOfblck8, blocksize2, QstepDC, QstepAC, INTER);
			reordering(cntFrm.blocks[nblck], numOfblck8, INTER);
			IQuantization_block(cntFrm.blocks[nblck], numOfblck8, blocksize2, QstepDC, QstepAC, INTER);
			IDPCM_DC_block(cntFrm, nblck, numOfblck8, blocksize2, splitWidth, INTER);
			IDCT_block(cntFrm.blocks[nblck], numOfblck8, blocksize2, INTER); // 복호된 err값을 반환
		}
		ImvPrediction(cntFrm, nblck);
		mergeBlock(cntFrm.blocks[nblck], blocksize2, INTER);		

		/* free */
		free(cntFrm.blocks[nblck].interDCTblck);
		free(cntFrm.blocks[nblck].interQuanblck);
		free(cntFrm.blocks[nblck].interInverseDCTblck);
	}
		
	interYReconstruct(cntFrm, prevFrm);
	interCbCr(cntFrm, prevFrm, QstepDC, QstepAC);
	fclose(gfp);

	for(int nblck=0; nblck<totalblck; nblck++)
	{
		for(int i=0; i<nblock8; i++)
			free(cntFrm.blocks[nblck].interInverseQuanblck[i]);
		free(cntFrm.blocks[nblck].interInverseQuanblck);
		free(cntFrm.blocks[nblck].interInverseErrblck16);
	}
	return 0;
}
void motionEstimation(FrameData& cntFrm, FrameData& prevFrm)
{
	int padlen = cntFrm.blocks->blocksize1;
	int blocksize1 = cntFrm.blocks->blocksize1;
	int blocksize2 = cntFrm.blocks->blocksize2;
	int width  = cntFrm.splitWidth*cntFrm.blocks->blocksize1;	// 352
	int height = cntFrm.splitHeight*cntFrm.blocks->blocksize1;  // 288
	int padImgWidth  = width + padlen * 2;	//384
	int padImgHeight = height + padlen * 2;	//320
	int splitWidth = cntFrm.splitWidth;
	int splitHeight = cntFrm.splitHeight;

	unsigned char* paddingImage = (unsigned char*) calloc(sizeof(unsigned char), padImgWidth*padImgHeight);
	// 패딩 이미지 생성
	getPaddingImage(prevFrm.reconstructedY, paddingImage, padImgWidth, padlen, width, height);


	unsigned char** spiralblck = (unsigned char**)malloc(sizeof(unsigned char*)*blocksize1);
	for(int i=0; i<blocksize1; i++)	spiralblck[i] = (unsigned char*)malloc(sizeof(unsigned char)*blocksize1);

	int y0=0, x0=0, cntX0=0, cntY0=0, tempX=0, tempY=0;
	int flag=0, xflag=1, yflag=-1;
	int xcnt=0, ycnt=0;
	int totalblck = cntFrm.nblocks16;
	int SADflag=1;
	int SAD=0, min=INT_MAX;
	int cnt=0;
	int nSearch = 64; // spiral search 비교회수

	//totalblck=1;
	for(int nblck=0; nblck<totalblck && SADflag; nblck++)
	{	
		Block16u *currentblck = cntFrm.blocks[nblck].originalblck16;
		min = INT_MAX; SAD = 0;	cnt = 0; SADflag=1;
		cntX0 = x0 = (nblck%splitWidth)*blocksize1;
		cntY0 = y0 = (nblck/splitWidth)*blocksize1;
		xcnt=0; ycnt=0;

		while(cnt<nSearch) // 전체 회수 64번 이더라; 수정될 가능성이 크다
		{	
			SAD = 0;
			if(!flag) // x변화
			{
				if(xflag<=0) x0 += xcnt;
				else		 x0 -= xcnt;
				flag=1; xcnt++; xflag*=-1;
			}
			else if(flag) // y변화
			{
				if(yflag<0)  y0 += ycnt;
				else		 y0 -= ycnt;
				flag=0;	ycnt++; yflag*=-1;
			}

			get16block(paddingImage, spiralblck, (padlen+y0), (padlen+x0), padImgWidth, blocksize1);
			SAD = getSAD(currentblck->block, spiralblck, blocksize1);

			if(min > SAD)
			{
				min = SAD;
				tempX = x0;
				tempY = y0;
			}
			else if(SAD==0)
			{
				tempX = x0;
				tempY = y0;
				break;
			}

			cnt++;
		}
		cntFrm.blocks[nblck].mv.x = cntX0 - tempX;
		cntFrm.blocks[nblck].mv.y = cntY0 - tempY;

	}

	for(int i=0; i<blocksize1; i++)
		free(spiralblck[i]);
	free(spiralblck);

	free(paddingImage);	
}
void motionCompensation(FrameData& cntFrm, FrameData& prevFrm)
{
	// prev.restructedY를 이용해서 prediction block을 만들자
	int totalblck  = cntFrm.nblocks16;
	int blocksize1 = cntFrm.blocks->blocksize1;
	int blocksize2 = cntFrm.blocks->blocksize2;
	int splitWidth = cntFrm.splitWidth;
	int width	   = cntFrm.splitWidth*blocksize1;
	int height	   = cntFrm.splitHeight*blocksize1;	
	int padlen = cntFrm.blocks->blocksize1;
	int padImgWidth  = width + padlen * 2;	//384
	int padImgHeight = height + padlen * 2;	//320


	unsigned char** predblck = (unsigned char**)malloc(sizeof(unsigned char*)*blocksize1);
	for(int i=0; i<blocksize1; i++) predblck[i] = (unsigned char *)malloc(sizeof(unsigned char)*blocksize1);

	// 패딩 이미지 생성
	unsigned char* paddingImage = (unsigned char*) calloc(sizeof(unsigned char), padImgWidth*padImgHeight);
	getPaddingImage(prevFrm.reconstructedY, paddingImage, padImgWidth, padlen, width, height);

	int refx=0, refy=0, cntx=0, cnty=0;
	for(int nblck=0; nblck<totalblck; nblck++)
	{

		Block16u *cntblck = cntFrm.blocks[nblck].originalblck16;
		cntx = (nblck%splitWidth)*blocksize1;
		cnty = (nblck/splitWidth)*blocksize1;
		refx = cntx - cntFrm.blocks[nblck].mv.x + padlen;
		refy = cnty - cntFrm.blocks[nblck].mv.y + padlen;		

		get16block(paddingImage, predblck, refy, refx, padImgWidth, blocksize1);	// reconstructedY는 inter에서 재생성한 이미지로 바꿔줘야함

		for(int y=0; y<blocksize1; y++)
		{
			for(int x=0; x<blocksize1; x++)
			{
				cntFrm.blocks[nblck].interErrblck16->block[y][x] = (int)(cntblck->block[y][x] - predblck[y][x]);
			}
		}

		for(int k=0; k<cntFrm.nblocks8; k++)
		{
			for(int y=0; y<blocksize2; y++)
			{
				for(int x=0; x<blocksize2; x++)
				{
					if(k==0)
						cntFrm.blocks[nblck].interErrblck8[k]->block[y][x] = cntFrm.blocks[nblck].interErrblck16->block[y][x];
					if(k==1)
						cntFrm.blocks[nblck].interErrblck8[k]->block[y][x] = cntFrm.blocks[nblck].interErrblck16->block[y][x+blocksize2];
					if(k==2)
						cntFrm.blocks[nblck].interErrblck8[k]->block[y][x] = cntFrm.blocks[nblck].interErrblck16->block[y+blocksize2][x];
					if(k==3)
						cntFrm.blocks[nblck].interErrblck8[k]->block[y][x] = cntFrm.blocks[nblck].interErrblck16->block[y+blocksize2][x+blocksize2];
				}
			}				
		}
	}


	for(int nblck=0; nblck<totalblck; nblck++)
		free(cntFrm.blocks[nblck].originalblck16);
	for(int i=0; i<blocksize1; i++) 
		free(predblck[i]);
	free(predblck);
	free(paddingImage);

	// 요기서 restructedY를 free하면 되지만 이후에 checkResultFrames에서 결과를 확인하기위해 restructedY를 참조하므로 free는 나중으로 미루자
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
void get16block(unsigned char* img, unsigned char *dst[16], int y0, int x0, int width, int blocksize)  
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
int getSAD(unsigned char currentblck[][16], unsigned char *spiralblck[16], int blocksize)
{
	int SAD = 0;
#if SIMD
	int SAD_SIMD = 0;
	__m256i spiralRow;
	__m256i crntRow;
	__m256i subRow;
	__m256i resRows[8];
	__m256i tempRows[2];
	__mmMIXED mixedRef;
	__mmMIXED mixedSrc;

	int nInterLoop = blocksize / 2; // 8번
	for (int i = 0; i < nInterLoop; i++)
	{
		mixedSrc.blck256 = _mm256_loadu_si256((__m256i*)currentblck[i*2]);
		mixedRef.blck256 = _mm256_loadu_si256((__m256i*)spiralblck[i*2]);

		crntRow = _mm256_cvtepu8_epi16(mixedSrc.blck128[0]);
		spiralRow = _mm256_cvtepu8_epi16(mixedRef.blck128[0]);
		subRow = _mm256_sub_epi16(crntRow, spiralRow);
		subRow = _mm256_abs_epi16(subRow);
		_mm256_storeu_si256(tempRows, subRow);

		crntRow = _mm256_cvtepu8_epi16(mixedSrc.blck128[1]);
		spiralRow = _mm256_cvtepu8_epi16(mixedRef.blck128[1]);
		subRow = _mm256_sub_epi16(crntRow, spiralRow);
		subRow = _mm256_abs_epi16(subRow);
		_mm256_storeu_si256(tempRows + 1, subRow);
		resRows[i] = _mm256_packs_epi16(tempRows[0], tempRows[1]);		
	}

	for(int i = 0; i < nInterLoop; i++)
		for (int j = 0; j < 32; j++)
			SAD_SIMD = (int)resRows[i].m256i_u8[i];

	SAD = SAD_SIMD;
#else
	// 16 x 16
	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			SAD += abs((int)(currentblck[y][x] - spiralblck[y][x]));
		}
	}
#endif
	fprintf(gfp, "%d\n", SAD);
	return SAD;
}
void interYReconstruct(FrameData& cntFrm, FrameData& prevFrm)
{
	int blocksize1  = cntFrm.blocks->blocksize1;
	int blocksize2  = cntFrm.blocks->blocksize2;
	int totalblck   = cntFrm.nblocks16;
	int nblck8      = cntFrm.nblocks8;
	int splitWidth  = cntFrm.splitWidth;
	int splitHeight = cntFrm.splitHeight;
	int width		= splitWidth*blocksize1;
	int height		= splitHeight*blocksize1;
	int temp		= 0;

	int padlen = cntFrm.blocks->blocksize1;
	int padImgWidth  = width  + padlen * 2;	//384
	int padImgHeight = height + padlen * 2;	//320	

	// 패딩 이미지 생성
	unsigned char* paddingImage = (unsigned char*) calloc(sizeof(unsigned char), padImgWidth*padImgHeight);
	getPaddingImage(prevFrm.reconstructedY, paddingImage, padImgWidth, padlen, width, height);

	unsigned char* Ychannel = (unsigned char*)calloc(width*height, sizeof(unsigned char));
	cntFrm.reconstructedY  = (unsigned char*) malloc(sizeof(unsigned char)*width*height);

	int refx=0, refy=0, cntx=0, cnty=0;
	int cntindex=0, refindex=0;
	for(int nblck=0; nblck<totalblck; nblck++)
	{
		BlockData &cntbd  = cntFrm.blocks[nblck];
		BlockData &prevbd = prevFrm.blocks[nblck];
		cntx = (nblck%splitWidth)*blocksize1;
		cnty = (nblck/splitWidth)*blocksize1;
		refx = cntx - cntFrm.blocks[nblck].Reconstructedmv.x + padlen;
		refy = cnty - cntFrm.blocks[nblck].Reconstructedmv.y + padlen;

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
	memcpy(cntFrm.reconstructedY, Ychannel, sizeof(unsigned char)*width*height);
	free(Ychannel);
}
void mvPrediction(FrameData& cntFrm, int numOfblck16)
{
	int blocksize = cntFrm.blocks->blocksize1;
	int totalblck = cntFrm.nblocks16;
	int splitWidth = cntFrm.splitWidth;

	int x1=0, x2=0, x3=0, xmedian=0;
	int y1=0, y2=0, y3=0, ymedian=0;

	BlockData& bd = cntFrm.blocks[numOfblck16];

	if(numOfblck16==0)
	{
		bd.mv.x = bd.mv.x-8;
		bd.mv.y = bd.mv.y-8;
	}
	else if(numOfblck16/splitWidth==0)
	{
		BlockData& prevbd = cntFrm.blocks[numOfblck16-1];
		bd.mv.x = bd.mv.x-prevbd.Reconstructedmv.x;
		bd.mv.y = bd.mv.y-prevbd.Reconstructedmv.y;
	}
	else if(numOfblck16%splitWidth==0)
	{			
		BlockData& prevbd = cntFrm.blocks[numOfblck16-splitWidth];
		bd.mv.x = bd.mv.x-prevbd.Reconstructedmv.x;
		bd.mv.y = bd.mv.y-prevbd.Reconstructedmv.y;
	}
	else
	{			
		if(numOfblck16%splitWidth==splitWidth-1)
		{
			// median l ul u
			x1 = cntFrm.blocks[numOfblck16-1].Reconstructedmv.x;			 // left
			x2 = cntFrm.blocks[numOfblck16-splitWidth-1].Reconstructedmv.x; // upper left
			x3 = cntFrm.blocks[numOfblck16-splitWidth].Reconstructedmv.x;   // upper

			if((x1>x2)&&(x1>x3))	  xmedian = (x2>x3)?x2:x3;
			else if((x2>x1)&&(x2>x3)) xmedian = (x1>x3)?x1:x3;
			else					  xmedian = (x1>x2)?x1:x2;

			y1 = cntFrm.blocks[numOfblck16-1].Reconstructedmv.y;			 // left
			y2 = cntFrm.blocks[numOfblck16-splitWidth-1].Reconstructedmv.y; // upper left
			y3 = cntFrm.blocks[numOfblck16-splitWidth].Reconstructedmv.y;   // upper

			if((y1>y2)&&(y1>y3))	  ymedian = (y2>y3)?y2:y3;
			else if((y2>y1)&&(y2>y3)) ymedian = (y1>x3)?y1:y3;
			else					  ymedian = (y1>y2)?y1:y2;
		}
		else
		{
			// median l u ur
			x1 = cntFrm.blocks[numOfblck16-1].Reconstructedmv.x;				// left
			x2 = cntFrm.blocks[numOfblck16-splitWidth].Reconstructedmv.x;		// upper
			x3 = cntFrm.blocks[numOfblck16-splitWidth+1].Reconstructedmv.x;    // upper right

			if((x1>x2)&&(x1>x3))	  xmedian = (x2>x3)?x2:x3;
			else if((x2>x1)&&(x2>x3)) xmedian = (x1>x3)?x1:x3;
			else					  xmedian = (x1>x2)?x1:x2;

			y1 = cntFrm.blocks[numOfblck16-1].Reconstructedmv.y;			   // left
			y2 = cntFrm.blocks[numOfblck16-splitWidth].Reconstructedmv.y;	   // upper
			y3 = cntFrm.blocks[numOfblck16-splitWidth+1].Reconstructedmv.y;   // upper right

			if((y1>y2)&&(y1>y3))	  ymedian = (y2>y3)?y2:y3;
			else if((y2>y1)&&(y2>y3)) ymedian = (y1>x3)?y1:y3;
			else					  ymedian = (y1>y2)?y1:y2;
		}
		bd.mv.x = bd.mv.x-xmedian;
		bd.mv.y = bd.mv.y-ymedian;
	}
	//cout << "차분 벡터값: " << bd.mv.x << ", " << bd.mv.y << endl;
}
void ImvPrediction(FrameData& cntFrm, int numOfblck16)
{
	int blocksize = cntFrm.blocks->blocksize1;
	int totalblck = cntFrm.nblocks16;
	int splitWidth =  cntFrm.splitWidth;
	int x1=0, x2=0, x3=0, xmedian=0;
	int y1=0, y2=0, y3=0, ymedian=0;


	BlockData& bd = cntFrm.blocks[numOfblck16];

	if(numOfblck16==0)
	{
		bd.Reconstructedmv.x = bd.mv.x+8;
		bd.Reconstructedmv.y = bd.mv.y+8;
	}
	else if(numOfblck16/splitWidth==0)
	{
		BlockData& prevbd = cntFrm.blocks[numOfblck16-1];
		bd.Reconstructedmv.x = bd.mv.x+prevbd.Reconstructedmv.x;
		bd.Reconstructedmv.y = bd.mv.y+prevbd.Reconstructedmv.y;
	}
	else if(numOfblck16%splitWidth==0)
	{			
		BlockData& prevbd = cntFrm.blocks[numOfblck16-splitWidth];
		bd.Reconstructedmv.x = bd.mv.x+prevbd.Reconstructedmv.x;
		bd.Reconstructedmv.y = bd.mv.y+prevbd.Reconstructedmv.y;
	}
	else
	{			
		if(numOfblck16%splitWidth==splitWidth-1)
		{
			// median l ul u
			x1 = cntFrm.blocks[numOfblck16-1].Reconstructedmv.x;			 // left
			x2 = cntFrm.blocks[numOfblck16-splitWidth-1].Reconstructedmv.x; // upper left
			x3 = cntFrm.blocks[numOfblck16-splitWidth].Reconstructedmv.x;   // upper

			if((x1>x2)&&(x1>x3))	  xmedian = (x2>x3)?x2:x3;
			else if((x2>x1)&&(x2>x3)) xmedian = (x1>x3)?x1:x3;
			else					  xmedian = (x1>x2)?x1:x2;

			y1 = cntFrm.blocks[numOfblck16-1].Reconstructedmv.y;			 // left
			y2 = cntFrm.blocks[numOfblck16-splitWidth-1].Reconstructedmv.y; // upper left
			y3 = cntFrm.blocks[numOfblck16-splitWidth].Reconstructedmv.y;   // upper

			if((y1>y2)&&(y1>y3))	  ymedian = (y2>y3)?y2:y3;
			else if((y2>y1)&&(y2>y3)) ymedian = (y1>x3)?y1:y3;
			else					  ymedian = (y1>y2)?y1:y2;
		}
		else
		{
			// median l u ur
			x1 = cntFrm.blocks[numOfblck16-1].Reconstructedmv.x;				// left
			x2 = cntFrm.blocks[numOfblck16-splitWidth].Reconstructedmv.x;		// upper
			x3 = cntFrm.blocks[numOfblck16-splitWidth+1].Reconstructedmv.x;    // upper right

			if((x1>x2)&&(x1>x3))	  xmedian = (x2>x3)?x2:x3;
			else if((x2>x1)&&(x2>x3)) xmedian = (x1>x3)?x1:x3;
			else					  xmedian = (x1>x2)?x1:x2;

			y1 = cntFrm.blocks[numOfblck16-1].Reconstructedmv.y;			   // left
			y2 = cntFrm.blocks[numOfblck16-splitWidth].Reconstructedmv.y;	   // upper
			y3 = cntFrm.blocks[numOfblck16-splitWidth+1].Reconstructedmv.y;   // upper right

			if((y1>y2)&&(y1>y3))	  ymedian = (y2>y3)?y2:y3;
			else if((y2>y1)&&(y2>y3)) ymedian = (y1>x3)?y1:y3;
			else					  ymedian = (y1>y2)?y1:y2;
		}
		bd.Reconstructedmv.x = bd.mv.x+xmedian;
		bd.Reconstructedmv.y = bd.mv.y+ymedian;
	}
	//cout << "복원 벡터값: " << bd.Reconstructedmv.x << ", " << bd.Reconstructedmv.y << endl;
	//system("pause");
}
void CmotionCompensation(FrameData& cntFrm, FrameData& prevFrm, int type)
{
	// padding image 생성
	// mv를 이용한 prediction block 생성
	// 차분영상 생성

	int totalblck  = cntFrm.nblocks16;
	int blocksize  = cntFrm.Cbblocks->blocksize;
	int splitWidth = cntFrm.CbCrSplitWidth;
	int width	   = cntFrm.CbCrSplitWidth*blocksize;
	int height	   = cntFrm.CbCrSplitHeight*blocksize;	
	int padlen     = cntFrm.Cbblocks->blocksize;	// padlen 8
	int padImgWidth  = width  + padlen * 2;		// padWidth  192
	int padImgHeight = height + padlen * 2;		// padHeight 160

	// make padding image
	unsigned char* paddingImage = (unsigned char*) calloc(sizeof(unsigned char), padImgWidth*padImgHeight);
	if(type == CB)
		getPaddingImage(prevFrm.reconstructedCb, paddingImage, padImgWidth, padlen, width, height); //restruct된 이전 cb, cr프레임
	else if(type == CR)
		getPaddingImage(prevFrm.reconstructedCr, paddingImage, padImgWidth, padlen, width, height); //restruct된 이전 cb, cr프레임

	// create prediction block
	unsigned char** predblck = (unsigned char**)malloc(sizeof(unsigned char*)*blocksize);
	for(int i=0; i<blocksize; i++) predblck[i] = (unsigned char *)malloc(sizeof(unsigned char)*blocksize);

	CBlockData *cbd = NULL;
 	if(type == CB)
		cbd = cntFrm.Cbblocks;
	else if(type == CR)
		cbd = cntFrm.Crblocks;

	int refx=0, refy=0, cntx=0, cnty=0;
	for(int nblck=0; nblck<totalblck; nblck++)
	{		
		cntx = (nblck%splitWidth)*blocksize;
		cnty = (nblck/splitWidth)*blocksize;
		refx = cntx - (cntFrm.blocks[nblck].Reconstructedmv.x/2) + padlen;
		refy = cnty - (cntFrm.blocks[nblck].Reconstructedmv.y/2) + padlen;		

		get16block(paddingImage, predblck, refy, refx, padImgWidth, blocksize);	// reconstructedY는 inter에서 재생성한 이미지로 바꿔줘야함

		for(int y=0; y<blocksize; y++)
		{
			for(int x=0; x<blocksize; x++)
			{
				cbd[nblck].interErrblck->block[y][x] = (int)(cbd[nblck].originalblck8->block[y][x] - predblck[y][x]);
			}
		}		
	}	

	for(int i=0; i<blocksize; i++) free(predblck[i]);
	free(predblck);
	free(paddingImage);
	for(int i=0; i<totalblck; i++)
		free(cbd[i].originalblck8);
}
void interCbCrReconstruct(FrameData& cntFrm, FrameData& prevFrm)
{
	int blocksize   = cntFrm.Cbblocks->blocksize;
	int totalblck   = cntFrm.nblocks16;
	int splitWidth  = cntFrm.CbCrSplitWidth;
	int splitHeight = cntFrm.CbCrSplitHeight;
	int width		= splitWidth*blocksize;		// 176
	int height		= splitHeight*blocksize;	// 144


	int padlen		 = cntFrm.Cbblocks->blocksize; // 8
	int padImgWidth  = width  + padlen * 2;	//192
	int padImgHeight = height + padlen * 2;	//160	

	// 패딩 이미지 생성
	unsigned char* paddingImageCb = (unsigned char*) calloc(sizeof(unsigned char), padImgWidth*padImgHeight);
	getPaddingImage(prevFrm.reconstructedCb, paddingImageCb, padImgWidth, padlen, width, height);
	cntFrm.reconstructedCb = (unsigned char*) malloc(sizeof(unsigned char)*width*height);

	unsigned char* paddingImageCr = (unsigned char*) calloc(sizeof(unsigned char), padImgWidth*padImgHeight);
	getPaddingImage(prevFrm.reconstructedCr, paddingImageCr, padImgWidth, padlen, width, height);
	cntFrm.reconstructedCr = (unsigned char*) malloc(sizeof(unsigned char)*width*height);

	unsigned char* Cbchannel = (unsigned char*)calloc(width*height, sizeof(unsigned char));
	unsigned char* Crchannel = (unsigned char*)calloc(width*height, sizeof(unsigned char));

	int tempCb=0, tempCr=0;
	int refx=0, refy=0, cntx=0, cnty=0;
	int cntindex=0, refindex=0;
	for(int nblck=0; nblck<totalblck; nblck++)
	{
		CBlockData &cntcbbd  = cntFrm.Cbblocks[nblck];
		CBlockData &prevcbbd = prevFrm.Cbblocks[nblck];
		CBlockData &cntcrbd  = cntFrm.Crblocks[nblck];
		CBlockData &prevcrbd = prevFrm.Crblocks[nblck];

		cntx = (nblck%splitWidth)*blocksize;
		cnty = (nblck/splitWidth)*blocksize;
		refx = cntx - (cntFrm.blocks[nblck].Reconstructedmv.x/2) + padlen;
		refy = cnty - (cntFrm.blocks[nblck].Reconstructedmv.y/2) + padlen;

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
	memcpy(cntFrm.reconstructedCb, Cbchannel, sizeof(unsigned char)*width*height);
	memcpy(cntFrm.reconstructedCr, Crchannel, sizeof(unsigned char)*width*height);

	free(paddingImageCb);
	free(paddingImageCr);
	free(Cbchannel);
	free(Crchannel);
}
void interCbCr(FrameData& cntFrm, FrameData& prevFrm, int QstepDC, int QstepAC)
{
	int totalblck = cntFrm.CbCrSplitHeight*cntFrm.CbCrSplitWidth;
	int blocksize = cntFrm.Cbblocks->blocksize;

	// motionEstimation -> dont need ME because CbCr motion vector can be gotten by Y motion vector / 2
	for(int i=0; i<totalblck; i++)
	{
		cntFrm.Cbblocks[i].interErrblck = (Block8i*)malloc(sizeof(Block8i));
		cntFrm.Crblocks[i].interErrblck = (Block8i*)malloc(sizeof(Block8i));
		cntFrm.Cbblocks[i].interInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
		cntFrm.Crblocks[i].interInverseQuanblck = (Block8i*)malloc(sizeof(Block8i));
		cntFrm.Cbblocks[i].interInverseDCTblck = (Block8d*)malloc(sizeof(Block8d));
		cntFrm.Crblocks[i].interInverseDCTblck = (Block8d*)malloc(sizeof(Block8d));
	}

	CmotionCompensation(cntFrm, prevFrm, CB);
	CmotionCompensation(cntFrm, prevFrm, CR);

	// 여기서부터 블록단위 반복 
	for(int nblck=0; nblck<totalblck; nblck++)
	{
		// Cb 할당 //
		cntFrm.Cbblocks[nblck].interDCTblck = (Block8d*)malloc(sizeof(Block8d));
		cntFrm.Cbblocks[nblck].interQuanblck = (Block8i*)malloc(sizeof(Block8i));

		CDCT_block(cntFrm.Cbblocks[nblck], blocksize, INTER);
		CDPCM_DC_block(cntFrm, cntFrm.Cbblocks[nblck], nblck, CB, INTER); 
		CQuantization_block(cntFrm.Cbblocks[nblck], blocksize, QstepDC, QstepAC, INTER);
		Creordering(cntFrm.Cbblocks[nblck], INTER);
		CIQuantization_block(cntFrm.Cbblocks[nblck], blocksize, QstepDC, QstepAC, INTER);
		CIDPCM_DC_block(cntFrm, cntFrm.Cbblocks[nblck], nblck, CB, INTER);
		CIDCT_block(cntFrm.Cbblocks[nblck], blocksize, INTER);


		// Cr 할당 //
		cntFrm.Crblocks[nblck].interDCTblck = (Block8d*)malloc(sizeof(Block8d));
		cntFrm.Crblocks[nblck].interQuanblck = (Block8i*)malloc(sizeof(Block8i));

		CDCT_block(cntFrm.Crblocks[nblck], blocksize, INTER);
		CDPCM_DC_block(cntFrm, cntFrm.Crblocks[nblck], nblck, CR, INTER); 
		CQuantization_block(cntFrm.Crblocks[nblck], blocksize, QstepDC, QstepAC, INTER);
		Creordering(cntFrm.Crblocks[nblck], INTER);
		CIQuantization_block(cntFrm.Crblocks[nblck], blocksize, QstepDC, QstepAC, INTER);
		CIDPCM_DC_block(cntFrm, cntFrm.Crblocks[nblck], nblck, CR, INTER);
		CIDCT_block(cntFrm.Crblocks[nblck], blocksize, INTER);
	}

	interCbCrReconstruct(cntFrm, prevFrm);

	for(int i=0; i<totalblck; i++)
	{
		free(cntFrm.Cbblocks[i].interInverseQuanblck);
		free(cntFrm.Crblocks[i].interInverseQuanblck);
		free(cntFrm.Cbblocks[i].interInverseDCTblck);
		free(cntFrm.Crblocks[i].interInverseDCTblck);
	}
}

/* common function */
void DCT_block(BlockData &bd , int numOfblck8, int blocksize, int type)
{
	Block8d *DCTblck = NULL;
	Block8i *Errblck = NULL;
	Block8d temp;
	if(type==INTRA)
	{
		DCTblck = (bd.intraDCTblck[numOfblck8]);
		Errblck = (bd.intraErrblck[numOfblck8]);
	}
	else if(type==INTER)
	{
		DCTblck = (bd.interDCTblck[numOfblck8]);
		Errblck = (bd.interErrblck8[numOfblck8]);
	}

	for(int x=0; x<blocksize; x++)
		for(int y=0; y<blocksize; y++)
			DCTblck->block[y][x] = temp.block[y][x] = 0;
			
#if SIMD
	// double type 시도 필요
	__m256 predictionRows[8];
	__m256 errRows[8];
	__m256 tempRows[8];
	for (int i = 0; i < 8; i++)
		predictionRows[i] = _mm256_loadu_ps(costable[i]);
	
	// 어째 연산량이 더 많아진 것 같다...
	// 실제로 시간을 측정해서 확인 필요
	float temp2[8][8] = { 0.f, };
	for (int u = 0; u < 8; u++)
	{		
		for(int j=0; j<blocksize; j++)
			errRows[j] = _mm256_cvtepi32_ps(_mm256_loadu_si256((__m256i*)Errblck->block[u]));

		for (int j = 0; j < 8; j++)
			tempRows[j] = _mm256_mul_ps(errRows[j], predictionRows[j]);

		for (int j = 0; j < blocksize; j++)
			for (int k = 0; k < blocksize; k++)
				temp2[u][j] += tempRows[j].m256_f32[k];
		
	}
	
	for (int u = 0; u < 8; u++)
	{
		for (int j = 0; j < 8; j++)
			errRows[j] = _mm256_setr_ps(temp2[0][u], temp2[1][u], temp2[2][u], temp2[3][u], temp2[4][u], temp2[5][u], temp2[6][u], temp2[7][u]);

		for (int j = 0; j < 8; j++)
			tempRows[j] = _mm256_mul_ps(errRows[j], predictionRows[j]);

		for (int j = 0; j < blocksize; j++)
			for (int k = 0; k < blocksize; k++)
				DCTblck->block[j][u] += tempRows[j].m256_f32[k];
	}
#else
	for(int v=0; v<blocksize; v++)
	{
		for(int u=0; u<blocksize; u++)
		{
			for(int x=0; x<blocksize; x++)
			{
				temp.block[v][u] += (double)Errblck->block[v][x] * costable[u][x];
			}
		}
	}

	for (int u = 0; u<blocksize; u++)
	{
		for (int v = 0; v<blocksize; v++)
		{
			for (int y = 0; y<blocksize; y++)
			{
				DCTblck->block[v][u] += temp.block[y][u] * costable[v][y];
			}
		}
	}
#endif

	for (int i = 0; i<blocksize; i++)
	{
		DCTblck->block[0][i] *= irt2;
		DCTblck->block[i][0] *= irt2;
	}

	for (int i = 0; i<blocksize; i++)
	{
		for (int j = 0; j<blocksize; j++)
		{
			DCTblck->block[i][j] *= (1. / 4.);
		}
	}


	free(Errblck);
	
}
void Quantization_block(BlockData &bd, int numOfblck8, int blocksize, int QstepDC, int QstepAC, int type)
{
	Block8d *DCTblck = NULL;  
	Block8i *Quanblck = NULL; 
	int *ACflag = NULL;

	if(type==INTRA)
	{
		DCTblck  = (bd.intraDCTblck[numOfblck8]);
		Quanblck = (bd.intraQuanblck[numOfblck8]);
		ACflag = &(bd.intraACflag[numOfblck8]);
	}
	else if(type==INTER)
	{
		DCTblck  = (bd.interDCTblck[numOfblck8]);
		Quanblck = (bd.interQuanblck[numOfblck8]);
		ACflag = &(bd.interACflag[numOfblck8]);
	}


	for(int y=0; y<blocksize; y++)
		for(int x=0; x<blocksize; x++)
			Quanblck->block[y][x] = 0;

	int Qstep=0;
	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			Qstep = (x==0&&y==0) ? QstepDC:QstepAC;
			Quanblck->block[y][x] = (int)floor(DCTblck->block[y][x] + 0.5) / Qstep;
		}
	}

	*ACflag = 1;
	for(int y=0; y<blocksize && (*ACflag); y++)
	{
		for(int x=0; x<blocksize && (*ACflag); x++)
		{
			if(x==0&&y==0) continue;
			*ACflag = (Quanblck->block[y][x]!=0)?0:1;
			//cout << y*blocksize+x << " ";
		}
	}
	//system("pause");
	//cout << *ACflag << " ";
	// DCT_block에서 저장된 Data는 이 함수말고 사용되는 곳이 없으므로 free
	free(DCTblck);
}
void IQuantization_block(BlockData &bd, int numOfblck8, int blocksize, int QstepDC, int QstepAC, int type)
{
	Block8i *Quanblck = NULL;
	Block8i *IQuanblck = NULL;
	if(type==INTRA)
	{
		Quanblck  = (bd.intraQuanblck[numOfblck8]);
		IQuanblck = (bd.intraInverseQuanblck[numOfblck8]);
	}
	else if(type==INTER)
	{
		Quanblck  = (bd.interQuanblck[numOfblck8]);
		IQuanblck = (bd.interInverseQuanblck[numOfblck8]);
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

	// data saved in Quantization doesn`s need after IQuantization Processing, So free
	free(Quanblck);
}
void IDCT_block(BlockData &bd, int numOfblck8, int blocksize, int type)
{
	Block8d *IDCTblck = NULL;
	Block8i *IQuanblck = NULL;
	Block8d temp;
	if(type==INTRA)
	{
		IDCTblck  = (bd.intraInverseDCTblck[numOfblck8]);
		IQuanblck = (bd.intraInverseQuanblck[numOfblck8]);
	}
	else if(type==INTER)
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

	//for(int i=0; i<blocksize; i++)
	//{
	//	for(int j=0; j<blocksize; j++)
	//	{
	//		idct2blck.block[i][j] *= (1./4.);
	//	}
	//}


	//cout << "dct2" << endl;
	//cout.precision(2);
	//for(int y=0; y<blocksize; y++)
	//{
	//	for(int x=0; x<blocksize; x++)
	//	{
	//		printf("%3.2lf ", idct2blck.block[y][x]);
	//	}
	//	cout << endl;
	//}
	//cout << endl << endl;

	/*cout << "dct1" << endl;
	for(int y=0; y<blocksize; y++)		
	{
		for(int x=0; x<blocksize; x++)
		{
			printf("%3.2lf ", IDCTblck->block[y][x]);
		}
		cout << endl;
	}
	cout << endl << endl;
	system("pause");
*/

	free(Cv);
	free(Cu);
	//free(IQuanblck);
}
void reordering(BlockData &bd, int numOfblck8, int predmode)
{
	int blocksize = bd.blocksize2;
	int *reorderedblck = (int*) calloc(blocksize*blocksize, sizeof(int));	// 해놓고 나중에 free; 나중에 모두 entropy 변환하고 free시키면 됨

	Block8i *Quanblck = NULL;
	if(predmode==INTRA)		 
	{
		Quanblck = (bd.intraQuanblck[numOfblck8]);
		bd.intraReorderedblck8[numOfblck8] = reorderedblck;
	}
	else if(predmode==INTER) 
	{
		Quanblck = (bd.interQuanblck[numOfblck8]);
		bd.interReorderedblck8[numOfblck8] = reorderedblck;
	}

	zigzagScanning(*Quanblck, reorderedblck, blocksize);
}
void Creordering(CBlockData &cbd,  int predmode)
{
	int blocksize = cbd.blocksize;
	int *reorderedblck = (int*) calloc(blocksize*blocksize, sizeof(int));	// 해놓고 나중에 free; 나중에 모두 entropy 변환하고 free시키면 됨

	Block8i *Quanblck = NULL;
	if(predmode==INTRA)		 
	{
		Quanblck = (cbd.intraQuanblck);
		cbd.intraReorderedblck = reorderedblck;
	}
	else if(predmode==INTER) 
	{
		Quanblck = (cbd.interQuanblck);
		cbd.interReorderedblck = reorderedblck;
	}
		
	CzigzagScanning(Quanblck, reorderedblck, blocksize);
}
void CzigzagScanning(Block8i *pQuanblck, int* dst, int blocksize)
{	
	Block8i &Quanblck = *pQuanblck;

	/*for(int nloop=1; nloop<=7; nloop+=2)
	{
		zzf(Quanblck, dst, nloop, beginidx, blocksize);
		beginidx+=nloop*2+1;
	}

	
	for(int nloop=6; nloop>=0; nloop-=2)
	{
		zzf(Quanblck, dst, nloop, beginidx, blocksize);
		beginidx+=nloop*2+1;
	}*/
	dst[0]	=	Quanblck.block[0][0];
	dst[1]	=	Quanblck.block[0][1];
	dst[2]	=	Quanblck.block[1][0];
	dst[3]	=	Quanblck.block[2][0];
	dst[4]	=	Quanblck.block[1][1];
	dst[5]	=	Quanblck.block[0][2];
	dst[6]	=	Quanblck.block[0][3];
	dst[7]	=	Quanblck.block[1][2];
	dst[8]	=	Quanblck.block[2][1];
	dst[9]	=	Quanblck.block[3][0];
	dst[10]	=	Quanblck.block[4][0];
	dst[11]	=	Quanblck.block[3][1];
	dst[12]	=	Quanblck.block[2][2];
	dst[13]	=	Quanblck.block[1][3];
	dst[14]	=	Quanblck.block[0][4];
	dst[15]	=	Quanblck.block[0][5];
	dst[16]	=	Quanblck.block[1][4];
	dst[17]	=	Quanblck.block[2][3];
	dst[18]	=	Quanblck.block[3][2];
	dst[19]	=	Quanblck.block[4][1];
	dst[20]	=	Quanblck.block[5][0];
	dst[21]	=	Quanblck.block[6][0];
	dst[22]	=	Quanblck.block[5][1];
	dst[23]	=	Quanblck.block[4][2];
	dst[24]	=	Quanblck.block[3][3];
	dst[25]	=	Quanblck.block[2][4];
	dst[26]	=	Quanblck.block[1][5];
	dst[27]	=	Quanblck.block[0][6];
	dst[28]	=	Quanblck.block[0][7];
	dst[29]	=	Quanblck.block[1][6];
	dst[30]	=	Quanblck.block[2][5];
	dst[31]	=	Quanblck.block[3][4];
	dst[32]	=	Quanblck.block[4][3];
	dst[33]	=	Quanblck.block[5][2];
	dst[34]	=	Quanblck.block[6][1];
	dst[35]	=	Quanblck.block[7][0];
	dst[36]	=	Quanblck.block[7][1];
	dst[37]	=	Quanblck.block[6][2];
	dst[38]	=	Quanblck.block[5][3];
	dst[39]	=	Quanblck.block[4][4];
	dst[40]	=	Quanblck.block[3][5];
	dst[41]	=	Quanblck.block[2][6];
	dst[42]	=	Quanblck.block[1][7];
	dst[43]	=	Quanblck.block[2][7];
	dst[44]	=	Quanblck.block[3][6];
	dst[45]	=	Quanblck.block[4][5];
	dst[46]	=	Quanblck.block[5][4];
	dst[47]	=	Quanblck.block[6][3];
	dst[48]	=	Quanblck.block[7][2];
	dst[49]	=	Quanblck.block[7][3];
	dst[50]	=	Quanblck.block[6][4];
	dst[51]	=	Quanblck.block[5][5];
	dst[52]	=	Quanblck.block[4][6];
	dst[53]	=	Quanblck.block[3][7];
	dst[54]	=	Quanblck.block[4][7];
	dst[55]	=	Quanblck.block[5][6];
	dst[56]	=	Quanblck.block[6][5];
	dst[57]	=	Quanblck.block[7][4];
	dst[58]	=	Quanblck.block[7][5];
	dst[59]	=	Quanblck.block[6][6];
	dst[60]	=	Quanblck.block[5][7];
	dst[61]	=	Quanblck.block[6][7];
	dst[62]	=	Quanblck.block[7][6];
	dst[63]	=	Quanblck.block[7][7];

}
void zigzagScanning(Block8i &Quanblck, int* dst, int blocksize)
{
	int beginidx=0;
	int nloop=1;

	/*for(int nloop=1; nloop<=7; nloop+=2)
	{
		zzf(Quanblck, dst, nloop, beginidx, blocksize);
		beginidx+=nloop*2+1;
	}

	for(int nloop=6; nloop>=0; nloop-=2)
	{
		zzf(Quanblck, dst, nloop, beginidx, blocksize);
		beginidx+=nloop*2+1;
	}*/
	
	dst[0]	=	Quanblck.block[0][0];
	dst[1]	=	Quanblck.block[0][1];
	dst[2]	=	Quanblck.block[1][0];
	dst[3]	=	Quanblck.block[2][0];
	dst[4]	=	Quanblck.block[1][1];
	dst[5]	=	Quanblck.block[0][2];
	dst[6]	=	Quanblck.block[0][3];
	dst[7]	=	Quanblck.block[1][2];
	dst[8]	=	Quanblck.block[2][1];
	dst[9]	=	Quanblck.block[3][0];
	dst[10]	=	Quanblck.block[4][0];
	dst[11]	=	Quanblck.block[3][1];
	dst[12]	=	Quanblck.block[2][2];
	dst[13]	=	Quanblck.block[1][3];
	dst[14]	=	Quanblck.block[0][4];
	dst[15]	=	Quanblck.block[0][5];
	dst[16]	=	Quanblck.block[1][4];
	dst[17]	=	Quanblck.block[2][3];
	dst[18]	=	Quanblck.block[3][2];
	dst[19]	=	Quanblck.block[4][1];
	dst[20]	=	Quanblck.block[5][0];
	dst[21]	=	Quanblck.block[6][0];
	dst[22]	=	Quanblck.block[5][1];
	dst[23]	=	Quanblck.block[4][2];
	dst[24]	=	Quanblck.block[3][3];
	dst[25]	=	Quanblck.block[2][4];
	dst[26]	=	Quanblck.block[1][5];
	dst[27]	=	Quanblck.block[0][6];
	dst[28]	=	Quanblck.block[0][7];
	dst[29]	=	Quanblck.block[1][6];
	dst[30]	=	Quanblck.block[2][5];
	dst[31]	=	Quanblck.block[3][4];
	dst[32]	=	Quanblck.block[4][3];
	dst[33]	=	Quanblck.block[5][2];
	dst[34]	=	Quanblck.block[6][1];
	dst[35]	=	Quanblck.block[7][0];
	dst[36]	=	Quanblck.block[7][1];
	dst[37]	=	Quanblck.block[6][2];
	dst[38]	=	Quanblck.block[5][3];
	dst[39]	=	Quanblck.block[4][4];
	dst[40]	=	Quanblck.block[3][5];
	dst[41]	=	Quanblck.block[2][6];
	dst[42]	=	Quanblck.block[1][7];
	dst[43]	=	Quanblck.block[2][7];
	dst[44]	=	Quanblck.block[3][6];
	dst[45]	=	Quanblck.block[4][5];
	dst[46]	=	Quanblck.block[5][4];
	dst[47]	=	Quanblck.block[6][3];
	dst[48]	=	Quanblck.block[7][2];
	dst[49]	=	Quanblck.block[7][3];
	dst[50]	=	Quanblck.block[6][4];
	dst[51]	=	Quanblck.block[5][5];
	dst[52]	=	Quanblck.block[4][6];
	dst[53]	=	Quanblck.block[3][7];
	dst[54]	=	Quanblck.block[4][7];
	dst[55]	=	Quanblck.block[5][6];
	dst[56]	=	Quanblck.block[6][5];
	dst[57]	=	Quanblck.block[7][4];
	dst[58]	=	Quanblck.block[7][5];
	dst[59]	=	Quanblck.block[6][6];
	dst[60]	=	Quanblck.block[5][7];
	dst[61]	=	Quanblck.block[6][7];
	dst[62]	=	Quanblck.block[7][6];
	dst[63]	=	Quanblck.block[7][7];

}
void zzf(Block8i &temp, int* dst, int nloop, int beginidx, int blocksize)
{
	int len = blocksize-1;
	if(nloop%2)
	{
		for(int i=0; i<nloop; i++) dst[beginidx+i] = temp.block[nloop-i-1][i];
		dst[beginidx+nloop] = temp.block[0][nloop];
		for(int i=0; i<nloop; i++) dst[(beginidx+nloop*2)-i] = temp.block[nloop-i][i];
	}
	else
	{
		for(int i=0; i<nloop; i++) dst[beginidx+i]	= temp.block[len-i][len-nloop+i];
		dst[beginidx+nloop] = temp.block[len][len-nloop];
		for(int i=0; i<nloop; i++) dst[(beginidx+nloop*2)-i] = temp.block[len-i][len-nloop+i+1];
	}
}
void entropyCoding(int* reordblck, int length)
{
	int value=0;
	char sign=1;
	char category[13]={0};

	for(int i=0; i<length; i++)
	{
		sign = (value>=0) ? 1:0;
		value = abs(reordblck[i]);
		if(value == 0)					     category[0]++;
		else if(value == 1)				     category[1]++;
		else if(value>=2    &&  value<=3)    category[2]++;
		else if(value>=4    &&  value<=7)    category[3]++;
		else if(value>=8    &&  value<=15)   category[4]++;
		else if(value>=16   &&  value<=31)   category[5]++;
		else if(value>=32   &&  value<=63)   category[6]++;
		else if(value>=64   &&  value<=127)  category[7]++;
		else if(value>=128  &&  value<=255)  category[8]++;
		else if(value>=256  &&  value<=511)  category[9]++;
		else if(value>=512  &&  value<=1023) category[10]++;
		else if(value>=1024 &&  value<=2047) category[11]++;
		else if(value>=2048)				 category[12]++;
	}



	int totalsize =0;
	int totalbytes=0;
	int lastindex =0;
	totalsize  = category[0]*2;
	totalsize += category[1]*4;
	totalsize += category[2]*5;
	totalsize += category[3]*6;
	totalsize += category[4]*7;
	totalsize += category[5]*8;
	totalsize += category[6]*10;
	totalsize += category[7]*12;
	totalsize += category[8]*14;
	totalsize += category[9]*16;
	totalsize += category[10]*18;
	totalsize += category[11]*20;
	totalsize += category[12]*22;

	totalbytes = (totalsize/8)+1; // 8bit로 딱 떨어지지 않을 경우를 대비해 +1;
	lastindex  = totalbytes-1;
	unsigned char* bitchar = (unsigned char*)calloc(sizeof(unsigned char), totalsize);
	//unsigned char* entropyResult = (unsigned char*)calloc(sizeof(unsigned char), (totalsize/8)+2);
	unsigned char* entropyResult = (unsigned char*)calloc(sizeof(unsigned char), (totalsize/8)+1); // EOF의 byte는 빼버림



	// 다시 돌아가면서 비트를 bitchar에 채워 넣으면 되겠다
	int idx=0;
	int bitcnt=0;
	int c=0;
	int exp=0;
	unsigned char etp=0;
	for(int i=0; i<length; i++)
	{
		value = abs(reordblck[i]);
		sign = (reordblck[i]>=0) ? 1:0;
		if(value == 0)					  
		{
			bitchar[idx++]=0;
			bitchar[idx++]=0;
		}
		else if(value == 1)
		{
			bitchar[idx++]=0;
			bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=sign;
		}
		else if(value>=2   &&  value<=3)
		{
			exp=1;
			bitchar[idx++]=0;
			bitchar[idx++]=1;
			bitchar[idx++]=1;
			bitchar[idx++]=sign;
			c=value-2;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;

		}
		else if(value>=4   &&  value<=7)
		{
			exp=2;
			bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=0;
			bitchar[idx++]=sign;

			c=value-4;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;
		}
		else if(value>=8   &&  value<=15)
		{
			exp=3;
			bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=1;
			bitchar[idx++]=sign;

			c=value-8;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;
		}
		else if(value>=16  &&  value<=31)
		{
			exp=4;
			bitchar[idx++]=1;
			bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=sign;

			c=value-16;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;
		}
		else if(value>=32  &&  value<=63)
		{
			exp=5;
			for(int n=0; n<exp-2; n++)
				bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=sign;

			c=value-32;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;
		}
		else if(value>=64  &&  value<=127)
		{
			exp=6;
			for(int n=0; n<exp-2; n++)
				bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=sign;

			c=value-64;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;
		}
		else if(value>=128 &&  value<=255)
		{
			exp=7;
			for(int n=0; n<exp-2; n++)
				bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=sign;

			c=value-128;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;
		}
		else if(value>=256 &&  value<=511)
		{
			exp=8;
			for(int n=0; n<exp-2; n++)
				bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=sign;

			c=value-256;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;
		}
		else if(value>=512 &&  value<=1023)
		{
			exp=9;
			for(int n=0; n<exp-2; n++)
				bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=sign;

			c=value-512;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;
		}
		else if(value>=1024 &&  value<=2047)
		{
			exp=10;
			for(int n=0; n<exp-2; n++)
				bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=sign;

			c=value-1024;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;
		}
		else if(value>=2048)
		{
			exp=11;
			for(int n=0; n<exp-2; n++)
				bitchar[idx++]=1;
			bitchar[idx++]=0;
			bitchar[idx++]=sign;

			c=value-2048;
			for(int n=exp; n>0; n--)
				bitchar[idx++] = (c>>(n-1))&1;
		}
	}


	unsigned char temp=0;
	for(int i=0; i<idx; i++)
	{
		temp |= bitchar[i];
		if((i+1)%8==0)
		{
			entropyResult[i/8] = temp;
		}		
		temp <<= 1;
	}
	temp >>= 1;

	entropyResult[totalbytes-1] = temp;

	int tmpbit=0;
	while((temp/=2) != 0)
		tmpbit++;

	entropyResult[totalbytes-1] <<= (8-tmpbit); // 순서대로 채워넣기위해 차지한 비트만큼 shift 시켜줌
	//entropyResult[totalbytes-1] = EOF; // 따로 넣지 않아도 프로그램이 종료될때 알아서 EOF가 들어가는 것 같다

	/*cout << "original value" << endl;
	for(int i=0; i<length; i++)
	{
	cout << (int)reordblck[i] << " ";
	}
	cout << endl;

	cout << "entropy result" << endl;
	for(int i=0; i<totalbytes; i++)
	{
	cout << (int)entropyResult[i] << " ";
	}
	cout << endl;

	cout << "bitchar" << endl;
	for(int i=0; i<length; i++)
	{
	cout << (int)bitchar[i] << " ";
	}
	cout << endl;*/
	FILE* fp = fopen("entropy.txt", "a+");
	fwrite(entropyResult, sizeof(char), totalbytes, fp);	
	fclose(fp);

	free(entropyResult);	
}
void entropyCoding(FrameData& frm, int predmode)
{
	int totalblck = frm.nblocks16;
	int nblck8 = frm.nblocks8;
	int value = 0;
	int blocksize = frm.blocks->blocksize2;
	int zzlength  = blocksize*blocksize;
	int category[13] = {0};
	
	int *reordblck;
	
	for(int nblck=0; nblck<totalblck; nblck++)
	{
		BlockData& bd = frm.blocks[nblck];
		for(int n8=0; n8<nblck8; n8++)
		{
			if(predmode==INTRA)
				reordblck = bd.intraReorderedblck8[n8];
			else if(predmode==INTER)
				reordblck = bd.interReorderedblck8[n8];

			for(int i=0; i<zzlength; i++)
			{
				value = abs(reordblck[i]);
				if(value == 0)					     category[0]++;
				else if(value == 1)				     category[1]++;
				else if(value>=2    &&  value<=3)    category[2]++;
				else if(value>=4    &&  value<=7)    category[3]++;
				else if(value>=8    &&  value<=15)   category[4]++;
				else if(value>=16   &&  value<=31)   category[5]++;
				else if(value>=32   &&  value<=63)   category[6]++;
				else if(value>=64   &&  value<=127)  category[7]++;
				else if(value>=128  &&  value<=255)  category[8]++;
				else if(value>=256  &&  value<=511)  category[9]++;
				else if(value>=512  &&  value<=1023) category[10]++;
				else if(value>=1024 &&  value<=2047) category[11]++;
				else if(value>=2048)				 category[12]++;
			}
		}
	}

	int totalbits  = 0;
	int totalbytes = 0;
	int lastindex  = 0;
	totalbits  = category[0]*2;
	totalbits += category[1]*4;
	totalbits += category[2]*5;
	totalbits += category[3]*6;
	totalbits += category[4]*7;
	totalbits += category[5]*8;
	totalbits += category[6]*10;
	totalbits += category[7]*12;
	totalbits += category[8]*14;
	totalbits += category[9]*16;
	totalbits += category[10]*18;
	totalbits += category[11]*20;
	totalbits += category[12]*22;

	//totalbytes = (totalbits/8)+1; // 8bit로 딱 떨어지지 않을 경우를 대비해 +1;
	//lastindex  = totalbytes-1;
	//bool* bitchar = (bool*)calloc(sizeof(bool), totalbits);
	//unsigned char* entropyResult = (unsigned char*)calloc(sizeof(unsigned char), (totalbits/8)+1); // EOF의 byte는 빼버림

	totalbytes = (totalbits/8)+2; // 8bit로 딱 떨어지지 않을 경우를 대비해 +1; EOF 삽입할 바이트 +1
	bool* bitchar = (bool*)calloc(sizeof(bool), totalbits);
	unsigned char* entropyResult = (unsigned char*)calloc(sizeof(unsigned char), totalbytes); // EOF의 byte포함

	int idx=0;
	int bitcnt=0;
	int c=0;
	int exp=0;
	int sign  = 0;	
	for(int nblck=0; nblck<totalblck; nblck++)
	{
		BlockData& bd = frm.blocks[nblck];
		for(int n8=0; n8<nblck8; n8++)
		{
			if(predmode==INTRA)
				reordblck = bd.intraReorderedblck8[n8];
			else if(predmode==INTER)
				reordblck = bd.interReorderedblck8[n8];

			for(int i=0; i<zzlength; i++)
			{
				value = abs(reordblck[i]);
				sign = (reordblck[i]>=0) ? 1:0;
				if(value == 0)					  
				{
					bitchar[idx++]=0;
					bitchar[idx++]=0;
				}
				else if(value == 1)
				{
					bitchar[idx++]=0;
					bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=sign;
				}
				else if(value>=2   &&  value<=3)
				{
					exp=1;
					bitchar[idx++]=0;
					bitchar[idx++]=1;
					bitchar[idx++]=1;
					bitchar[idx++]=sign;
					c=value-2;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;

				}
				else if(value>=4   &&  value<=7)
				{
					exp=2;
					bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=0;
					bitchar[idx++]=sign;

					c=value-4;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;
				}
				else if(value>=8   &&  value<=15)
				{
					exp=3;
					bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=1;
					bitchar[idx++]=sign;

					c=value-8;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;
				}
				else if(value>=16  &&  value<=31)
				{
					exp=4;
					bitchar[idx++]=1;
					bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=sign;

					c=value-16;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;
				}
				else if(value>=32  &&  value<=63)
				{
					exp=5;
					for(int n=0; n<exp-2; n++)
						bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=sign;

					c=value-32;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;
				}
				else if(value>=64  &&  value<=127)
				{
					exp=6;
					for(int n=0; n<exp-2; n++)
						bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=sign;

					c=value-64;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;
				}
				else if(value>=128 &&  value<=255)
				{
					exp=7;
					for(int n=0; n<exp-2; n++)
						bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=sign;

					c=value-128;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;
				}
				else if(value>=256 &&  value<=511)
				{
					exp=8;
					for(int n=0; n<exp-2; n++)
						bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=sign;

					c=value-256;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;
				}
				else if(value>=512 &&  value<=1023)
				{
					exp=9;
					for(int n=0; n<exp-2; n++)
						bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=sign;

					c=value-512;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;
				}
				else if(value>=1024 &&  value<=2047)
				{
					exp=10;
					for(int n=0; n<exp-2; n++)
						bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=sign;

					c=value-1024;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;
				}
				else if(value>=2048)
				{
					exp=11;
					for(int n=0; n<exp-2; n++)
						bitchar[idx++]=1;
					bitchar[idx++]=0;
					bitchar[idx++]=sign;

					c=value-2048;
					for(int n=exp; n>0; n--)
						bitchar[idx++] = (c>>(n-1))&1;
				}
			}
		}
	}

	unsigned char temp=0;
	for(int i=0; i<idx; i++)
	{
		temp |= bitchar[i];
		if((i+1)%8==0)
		{
			entropyResult[i/8] = temp;
		}		
		temp <<= 1;
	}
	temp >>= 1;

	int tmpbit=0;
	while((temp/=2) != 0)
		tmpbit++;

	entropyResult[totalbytes-2] <<= (7-tmpbit); // 순서대로 채워넣기위해 차지한 비트만큼 shift 시켜줌
	entropyResult[totalbytes-1] = EOF;
		
	FILE* fp = fopen("entropy.bin", "a+");
	fwrite(entropyResult, sizeof(unsigned char), totalbytes, fp);
	//cb cr 추가
	fclose(fp);

	/*free*/
	for(int nblck=0; nblck<totalblck; nblck++)
	{
		BlockData& bd = frm.blocks[nblck];
		for(int n8=0; n8<nblck8; n8++)
		{
			if(predmode==INTRA)
				reordblck = bd.intraReorderedblck8[n8];
			else if(predmode==INTER)
				reordblck = bd.interReorderedblck8[n8];
			free(reordblck);
		}
	}
	free(bitchar);
	free(entropyResult);
}
void DPCM_DC_block(FrameData &frm, int numOfblck16, int numOfblck8, int blocksize, int splitWidth, int predmode)
{
	int a=0, b=0, c=0;
	int median = 0; 
	int numOfCurrentBlck=0;

	BlockData& bd = frm.blocks[numOfblck16];
	if(predmode==INTRA)
	{
		if(numOfblck16 == 0) // 16x16블록의 위치가 첫 번째이다
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// DC value - 1024
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - 1024;
				break;
			case 1:
				numOfCurrentBlck=1;
				// DC value - DC value of left block
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - bd.intraInverseQuanblck[0]->block[0][0];
				break;
			case 2:
				numOfCurrentBlck=2;
				// DC value - DC value of upper block
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - bd.intraInverseQuanblck[0]->block[0][0];
				break;
			case 3:
				numOfCurrentBlck=3;
				// DC value - median value of DC value among left, upper left, upper right
				a = bd.intraInverseQuanblck[2]->block[0][0]; // left
				b = bd.intraInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - median;
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
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - frm.blocks[numOfblck16-1].intraInverseQuanblck[1]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// dc - left dc
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - bd.intraInverseQuanblck[0]->block[0][0];
				break;
			case 2:
				numOfCurrentBlck=2;
				// median
				a = frm.blocks[numOfblck16-1].intraInverseQuanblck[3]->block[0][0]; // left
				b = bd.intraInverseQuanblck[0]->block[0][0]; // upper 
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper right
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - median;
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
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			}
		}
		else if(numOfblck16%splitWidth == 0) // 16x16 블록의 위치가 첫 열이다.
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// DC - upper
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[2]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// median (l u ur)
				a = bd.intraInverseQuanblck[0]->block[0][0]; // left
				b = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0]; // upper
				c = frm.blocks[numOfblck16-splitWidth+1].intraInverseQuanblck[2]->block[0][0]; // upper right
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			case 2:
				numOfCurrentBlck=2;
				// upper
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - bd.intraInverseQuanblck[0]->block[0][0];
				break;
			case 3:
				numOfCurrentBlck=3;
				//median (l ul u)
				a = bd.intraInverseQuanblck[2]->block[0][0]; // left
				b = bd.intraInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - median;
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
				a = frm.blocks[numOfblck16-1].intraInverseQuanblck[1]->block[0][0];
				b = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[2]->block[0][0];
				c = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0];
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			case 1:
				numOfCurrentBlck=1;
				// median (l u ur); 끝 블럭은 (l ul u)으로 해줘야 됨
				if(numOfblck16%splitWidth == splitWidth-1)
				{
					a = bd.intraInverseQuanblck[0]->block[0][0];	// left
					b = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[2]->block[0][0]; // upper left
					c = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0]; // upper 
				}
				else
				{
					a = bd.intraInverseQuanblck[0]->block[0][0];	// left
					b = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0]; // upper
					c = frm.blocks[numOfblck16-splitWidth+1].intraInverseQuanblck[2]->block[0][0]; // upper right
				}
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			case 2:
				numOfCurrentBlck=2;
				// median (l u ur);
				a = frm.blocks[numOfblck16-1].intraInverseQuanblck[3]->block[0][0];	// left
				b = bd.intraInverseQuanblck[0]->block[0][0];	// upper
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper left
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			case 3:
				numOfCurrentBlck=3;
				// median (l ul u)
				a = bd.intraInverseQuanblck[2]->block[0][0];	// left
				b = bd.intraInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.intraInverseQuanblck[1]->block[0][0]; // upper 
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraDCTblck[numOfCurrentBlck]->block[0][0] = bd.intraDCTblck[numOfCurrentBlck]->block[0][0] - median;
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
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - 1024;
				break;
			case 1:
				numOfCurrentBlck=1;
				// DC value - DC value of left block
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - bd.interInverseQuanblck[0]->block[0][0];
				break;
			case 2:
				numOfCurrentBlck=2;
				// DC value - DC value of upper block
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - bd.interInverseQuanblck[0]->block[0][0];
				break;
			case 3:
				numOfCurrentBlck=3;
				// DC value - median value of DC value among left, upper left, upper right
				a = bd.interInverseQuanblck[2]->block[0][0]; // left
				b = bd.interInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - median;
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
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - frm.blocks[numOfblck16-1].interInverseQuanblck[1]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// dc - left dc
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - bd.interInverseQuanblck[0]->block[0][0];
				break;
			case 2:
				numOfCurrentBlck=2;
				// median
				a = frm.blocks[numOfblck16-1].interInverseQuanblck[3]->block[0][0]; // left
				b = bd.interInverseQuanblck[0]->block[0][0]; // upper 
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper right
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - median;
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
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			}
		}
		else if(numOfblck16%splitWidth == 0) // 16x16 블록의 위치가 첫 열이다.
		{
			switch(numOfblck8)
			{
			case 0:
				numOfCurrentBlck=0;
				// DC - upper
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[2]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// median (l u ur)
				a = bd.interInverseQuanblck[0]->block[0][0]; // left
				b = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0]; // upper
				c = frm.blocks[numOfblck16-splitWidth+1].interInverseQuanblck[2]->block[0][0]; // upper right
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			case 2:
				numOfCurrentBlck=2;
				// upper
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - bd.interInverseQuanblck[0]->block[0][0];
				break;
			case 3:
				numOfCurrentBlck=3;
				//median (l ul u)
				a = bd.interInverseQuanblck[2]->block[0][0]; // left
				b = bd.interInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - median;
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
				a = frm.blocks[numOfblck16-1].interInverseQuanblck[1]->block[0][0];
				b = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[2]->block[0][0];
				c = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0];
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			case 1:
				numOfCurrentBlck=1;
				// median (l u ur); 끝 블럭은 (l ul u)으로 해줘야 됨
				if(numOfblck16%splitWidth == splitWidth-1)
				{
					a = bd.interInverseQuanblck[0]->block[0][0];	// left
					b = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[2]->block[0][0]; // upper left
					c = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0]; // upper 
				}
				else
				{
					a = bd.interInverseQuanblck[0]->block[0][0];	// left
					b = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0]; // upper
					c = frm.blocks[numOfblck16-splitWidth+1].interInverseQuanblck[2]->block[0][0]; // upper right
				}
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			case 2:
				numOfCurrentBlck=2;
				// median (l u ur);
				a = frm.blocks[numOfblck16-1].interInverseQuanblck[3]->block[0][0];	// left
				b = bd.interInverseQuanblck[0]->block[0][0];	// upper
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper left
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			case 3:
				numOfCurrentBlck=3;
				// median (l ul u)
				a = bd.interInverseQuanblck[2]->block[0][0];	// left
				b = bd.interInverseQuanblck[0]->block[0][0]; // upper left
				c = bd.interInverseQuanblck[1]->block[0][0]; // upper 
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interDCTblck[numOfCurrentBlck]->block[0][0] = bd.interDCTblck[numOfCurrentBlck]->block[0][0] - median;
				break;
			}
		}
	}
}
void IDPCM_DC_block(FrameData &frm, int numOfblck16, int numOfblck8, int blocksize, int splitWidth, int predmode)
{
	int a=0, b=0, c=0;
	int median=0;
	int numOfCurrentBlck=0;

	BlockData& bd = frm.blocks[numOfblck16];

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
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + frm.blocks[numOfblck16-1].intraInverseQuanblck[1]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// dc - left dc
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + bd.intraInverseQuanblck[0]->block[0][0];
				break;
			case 2:
				numOfCurrentBlck=2;
				// median
				a = frm.blocks[numOfblck16-1].intraInverseQuanblck[3]->block[0][0]; // left
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
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[2]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// median (l u ur)
				a = bd.intraInverseQuanblck[0]->block[0][0]; // left
				b = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0]; // upper
				c = frm.blocks[numOfblck16-splitWidth+1].intraInverseQuanblck[2]->block[0][0]; // upper right
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
				a = frm.blocks[numOfblck16-1].intraInverseQuanblck[1]->block[0][0];
				b = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[2]->block[0][0];
				c = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0];
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
					b = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[2]->block[0][0]; // upper left
					c = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0]; // upper 
				}
				else
				{
					a = bd.intraInverseQuanblck[0]->block[0][0];	// left
					b = frm.blocks[numOfblck16-splitWidth].intraInverseQuanblck[3]->block[0][0]; // upper
					c = frm.blocks[numOfblck16-splitWidth+1].intraInverseQuanblck[2]->block[0][0]; // upper right
				}
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.intraInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 2:
				numOfCurrentBlck=2;
				// median (l u ur);
				a = frm.blocks[numOfblck16-1].intraInverseQuanblck[3]->block[0][0];	// left
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
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + frm.blocks[numOfblck16-1].interInverseQuanblck[1]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// dc - left dc
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + bd.interInverseQuanblck[0]->block[0][0];
				break;
			case 2:
				numOfCurrentBlck=2;
				// median
				a = frm.blocks[numOfblck16-1].interInverseQuanblck[3]->block[0][0]; // left
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
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[2]->block[0][0];
				break;
			case 1:
				numOfCurrentBlck=1;
				// median (l u ur)
				a = bd.interInverseQuanblck[0]->block[0][0]; // left
				b = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0]; // upper
				c = frm.blocks[numOfblck16-splitWidth+1].interInverseQuanblck[2]->block[0][0]; // upper right
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
				a = frm.blocks[numOfblck16-1].interInverseQuanblck[1]->block[0][0];
				b = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[2]->block[0][0];
				c = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0];
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
					b = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[2]->block[0][0]; // upper left
					c = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0]; // upper 
				}
				else
				{
					a = bd.interInverseQuanblck[0]->block[0][0];	// left
					b = frm.blocks[numOfblck16-splitWidth].interInverseQuanblck[3]->block[0][0]; // upper
					c = frm.blocks[numOfblck16-splitWidth+1].interInverseQuanblck[2]->block[0][0]; // upper right
				}
				if( (a>b) && (a>c) )	 median=(b>c) ? b:c;
				else if((b>a) && (b>c))	 median=(a>c) ? a:c;
				else		   			 median=(a>b) ? a:b;
				bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] = bd.interInverseQuanblck[numOfCurrentBlck]->block[0][0] + median;
				break;
			case 2:
				numOfCurrentBlck=2;
				// median (l u ur);
				a = frm.blocks[numOfblck16-1].interInverseQuanblck[3]->block[0][0];	// left
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
void CDCT_block(CBlockData &bd, int blocksize, int predmode)
{
	Block8d *DCTblck = NULL;
	Block8i temp;
	Block8i *Errblck = &temp;
	Block8d dcttemp;
	if(predmode==INTRA)
	{
		DCTblck = (bd.intraDCTblck);
		for(int y=0; y<blocksize; y++)
			for(int x=0; x<blocksize; x++)
				Errblck->block[y][x] = (int)bd.originalblck8->block[y][x];

	}
	else if(predmode==INTER)
	{
		DCTblck = (bd.interDCTblck);
		Errblck = (bd.interErrblck);
	}
		
	for(int y=0; y<blocksize; y++)
		for(int x=0; x<blocksize; x++)
			DCTblck->block[y][x] = dcttemp.block[y][x] = 0;

	/*for(int v=0; v<blocksize; v++)
	{
		for(int u=0; u<blocksize; u++)
		{
			for(int x=0; x<blocksize; x++)
			{
				for(int y=0; y<blocksize; y++)
				{
					DCTblck->block[v][u] += (double)Errblck->block[y][x] * cos(((2*x+1)*u*pi)/16.) * cos(((2*y+1)*v*pi)/16.);
				}
			}
		}
	}*/


	for(int v=0; v<blocksize; v++)
	{
		for(int u=0; u<blocksize; u++)
		{
			for(int x=0; x<blocksize; x++)
			{
				dcttemp.block[v][u] += (double)Errblck->block[v][x] * costable[u][x];
			}
		}
	}

	for(int u=0; u<blocksize; u++)
	{
		for(int v=0; v<blocksize; v++)
		{		
			for(int y=0; y<blocksize; y++)
			{
				DCTblck->block[v][u] += dcttemp.block[y][u] * costable[v][y];
			}
		}
	}

	for(int i=0; i<blocksize; i++)
	{
		DCTblck->block[0][i] *= irt2;
		DCTblck->block[i][0] *= irt2;
	}

	for(int i=0; i<blocksize; i++)
	{
		for(int j=0; j<blocksize; j++)
		{
			DCTblck->block[i][j] *= (1./4.);
		}
	}

	// splitBlocks에서 할당
	if(predmode==INTRA)
		free(bd.originalblck8);
	else if(predmode==INTER)
		free(Errblck);

}
void CDPCM_DC_block(FrameData& frm, CBlockData& bd, int numOfblck8, int CbCrtype, int predmode)
{
	int splitWidth = frm.CbCrSplitWidth;
	int numOfCurrentBlck =0;
	int a=0, b=0, c=0, median=0;

	CBlockData* cbd = NULL;
	if(CbCrtype==CB) cbd = frm.Cbblocks;
	else if(CbCrtype==CR) cbd = frm.Crblocks;

	Block8d *DCTblck = NULL;
	Block8i *IQuanblckLeft = NULL;
	Block8i *IQuanblckUpper = NULL;
	Block8i *IQuanblckUpperLeft = NULL;
	Block8i *IQuanblckUpperRight = NULL;
	if(predmode==INTRA)
	{
		if(numOfblck8==0)
			DCTblck = (bd.intraDCTblck);
		else if(numOfblck8/splitWidth==0)
		{
			DCTblck = (bd.intraDCTblck);
			IQuanblckLeft = (cbd[numOfblck8-1].intraInverseQuanblck);
		}
		else if(numOfblck8%splitWidth==0)
		{
			DCTblck = (bd.intraDCTblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].intraInverseQuanblck);
		}
		else 
		{
			DCTblck = (bd.intraDCTblck);
			IQuanblckLeft = (cbd[numOfblck8-1].intraInverseQuanblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].intraInverseQuanblck);
			IQuanblckUpperLeft = (cbd[numOfblck8-splitWidth-1].intraInverseQuanblck);
			IQuanblckUpperRight = (cbd[numOfblck8-splitWidth+1].intraInverseQuanblck);
		}
	}
	else if(predmode==INTER)
	{
		if(numOfblck8==0)
			DCTblck = (bd.interDCTblck);
		else if(numOfblck8/splitWidth==0)
		{
			DCTblck = (bd.interDCTblck);
			IQuanblckLeft = (cbd[numOfblck8-1].interInverseQuanblck);
		}
		else if(numOfblck8%splitWidth==0)
		{
			DCTblck = (bd.interDCTblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].interInverseQuanblck);
		}
		else 
		{
			DCTblck = (bd.interDCTblck);
			IQuanblckLeft = (cbd[numOfblck8-1].interInverseQuanblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].interInverseQuanblck);
			IQuanblckUpperLeft = (cbd[numOfblck8-splitWidth-1].interInverseQuanblck);
			IQuanblckUpperRight = (cbd[numOfblck8-splitWidth+1].interInverseQuanblck);
		}
	}

	if(numOfblck8==0)
	{
		DCTblck->block[0][0] = DCTblck->block[0][0] - 1024;
	}
	else if(numOfblck8/splitWidth==0) // 첫 행
	{
		DCTblck->block[0][0] = DCTblck->block[0][0] - IQuanblckLeft->block[0][0];
	}
	else if(numOfblck8%splitWidth==0) // 첫 열
	{
		DCTblck->block[0][0] = DCTblck->block[0][0] - IQuanblckUpper->block[0][0];
	}
	else
	{
		// median
		if(numOfblck8%splitWidth == splitWidth-1)
		{
			a = IQuanblckLeft->block[0][0];				// left
			b = IQuanblckUpperLeft->block[0][0];  // upper left
			c = IQuanblckUpper->block[0][0];    // upper
		}
		else
		{
			a = IQuanblckLeft->block[0][0];				// left
			b = IQuanblckUpper->block[0][0];	// upper 
			c = IQuanblckUpperRight->block[0][0];  // upper right
		}
		if((a>b)&&(a>c))		median=(b>c)?b:c;
		else if((b>a)&&(b>c))   median=(a>c)?a:c;
		else					median=(a>b)?a:b;
		DCTblck->block[0][0] = DCTblck->block[0][0] - median;
	}
}
void CIDPCM_DC_block(FrameData& frm, CBlockData& bd, int numOfblck8, int CbCrtype, int predmode)
{
	int splitWidth = frm.CbCrSplitWidth;
	int numOfCurrentBlck =0;
	int a=0, b=0, c=0, median=0;

	CBlockData* cbd = NULL;
	if(CbCrtype==CB) cbd = frm.Cbblocks;
	else if(CbCrtype==CR) cbd = frm.Crblocks;

	Block8i *IQuanblck = NULL;
	Block8i *IQuanblckLeft = NULL;
	Block8i *IQuanblckUpper = NULL;
	Block8i *IQuanblckUpperLeft = NULL;
	Block8i *IQuanblckUpperRight = NULL;
	if(predmode==INTRA)
	{
		if(numOfblck8==0)
			IQuanblck = (bd.intraInverseQuanblck);
		else if(numOfblck8/splitWidth==0)
		{
			IQuanblck = (bd.intraInverseQuanblck);
			IQuanblckLeft = (cbd[numOfblck8-1].intraInverseQuanblck);
		}
		else if(numOfblck8%splitWidth==0)
		{
			IQuanblck = (bd.intraInverseQuanblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].intraInverseQuanblck);
		}
		else
		{
			IQuanblck = (bd.intraInverseQuanblck);
			IQuanblckLeft = (cbd[numOfblck8-1].intraInverseQuanblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].intraInverseQuanblck);
			IQuanblckUpperLeft = (cbd[numOfblck8-splitWidth-1].intraInverseQuanblck);
			IQuanblckUpperRight = (cbd[numOfblck8-splitWidth+1].intraInverseQuanblck);
		}
	}
	else if(predmode==INTER)
	{
		if(numOfblck8==0)
			IQuanblck = (bd.interInverseQuanblck);
		else if(numOfblck8/splitWidth==0)
		{
			IQuanblck = (bd.interInverseQuanblck);
			IQuanblckLeft = (cbd[numOfblck8-1].interInverseQuanblck);
		}
		else if(numOfblck8%splitWidth==0)
		{
			IQuanblck = (bd.interInverseQuanblck);
			IQuanblckUpper = (cbd[numOfblck8-splitWidth].interInverseQuanblck);
		}
		else
		{
			IQuanblck = (bd.interInverseQuanblck);
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
void CQuantization_block(CBlockData &bd, int blocksize, int QstepDC, int QstepAC, int predmode)
{
	Block8d *DCTblck = NULL;
	Block8i *Quanblck = NULL;
	int *ACflag = NULL;
	if(predmode==INTRA)
	{
		DCTblck  = (bd.intraDCTblck);
		Quanblck = (bd.intraQuanblck);
		ACflag = &(bd.intraACflag);
	}
	else if(predmode==INTER)
	{
		DCTblck  = (bd.interDCTblck);
		Quanblck = (bd.interQuanblck);
		ACflag = &(bd.interACflag);
	}

	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			Quanblck->block[y][x] = 0;
		}
	}

	int Qstep = 0;
	for(int y=0; y<blocksize; y++)
	{
		for(int x=0; x<blocksize; x++)
		{
			Qstep = (x==0&&y==0)? QstepDC:QstepAC;
			Quanblck->block[y][x] = (int)floor(DCTblck->block[y][x] + 0.5) / Qstep  ;
		}
	}

	*ACflag = 1;
	for(int y=0; y<blocksize && (*ACflag); y++)
	{
		for(int x=0; x<blocksize && (*ACflag); x++)
		{
			if(x==0&&y==0) continue;
			*ACflag = (Quanblck->block[y][x]!=0)?0:1;
		}
	}
	free(DCTblck);
}
void CIQuantization_block(CBlockData &bd, int blocksize, int QstepDC, int QstepAC, int predmode)
{
	Block8i *IQuanblck = NULL;
	Block8i *Quanblck = NULL;
	if(predmode==INTRA)
	{
		IQuanblck = (bd.intraInverseQuanblck);
		Quanblck = (bd.intraQuanblck);
	}
	else if(predmode==INTER)
	{
		IQuanblck = (bd.interInverseQuanblck);
		Quanblck = (bd.interQuanblck);
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
void CIDCT_block(CBlockData &bd, int blocksize, int predmode)
{
	Block8d *IDCTblck = NULL;
	Block8i *IQuanblck = NULL;
	Block8d temp;
	if(predmode==INTRA)
	{
		IDCTblck = (bd.intraInverseDCTblck);
		IQuanblck = (bd.intraInverseQuanblck);
	}
	else if(predmode==INTER)
	{
		IDCTblck  = (bd.interInverseDCTblck);
		IQuanblck = (bd.interInverseQuanblck);
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

	free(Cv);
	free(Cu);
}
void mergeBlock(BlockData &bd, int blocksize, int type) // 8x8 -> 16x16 으로 만들기
{	
	int nblck8 = 4;
	if(type==INTRA)
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
	else if(type==INTER)
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


/* bitstream function */
void makebitstream(FrameData* frames, int nframes, int height, int width, int QstepDC, int QstepAC, int intraPeriod, int predmode)
{
	/* header */
	header hd;
	headerinit(hd, height, width, QstepDC, QstepAC, intraPeriod);
	char compCIFfname[256];
	sprintf(compCIFfname, "output\\%s_compCIF_%d_%d_%d.bin", filename, QstepDC, QstepAC, intraPeriod);
	#pragma pack(push, 1)
	FILE* fp = fopen(compCIFfname, "wb");
	if(fp==NULL)
	{
		cout << "fail to open compCIF.bin" << endl;
		exit(-1);
	}
	fwrite(&hd, sizeof(header), 1, fp);
	#pragma pack(pop)
	
	/* body */
	if(predmode==INTRA)
	{
		allintraBody(frames, nframes, fp);
	}
	else if(predmode==INTER)
	{
		// 여기서 메모리를 만들어 놓고, intraBody와 interBody에 전달해서 메모리 사용하게 한후 끝나고 밖에서 반환
		int cntbits = 0;
		int maxbits = frames->splitHeight * frames->splitWidth * frames->blocks->blocksize1 * frames->blocks->blocksize1 * 8 * nframes;
		unsigned char* tempFrame = (unsigned char*)malloc(sizeof(unsigned char)*(maxbits/8));
		if(tempFrame==NULL)
		{
			cout << "fail to allocate memory in makebitstream" << endl;
			exit(-1);
		}

		for(int n=0; n<nframes; n++)
		{
			if(n%intraPeriod==0)
			{
				intraBody(frames[n], tempFrame, cntbits);
				//cout << "intra frame bits: " << cntbits << endl;
			}
			else
			{
				interBody(frames[n], tempFrame, cntbits);
				//cout << "inter frame bits: " << cntbits << endl;
				
			}
		}
		fwrite(tempFrame, (cntbits/8)+1, 1, fp);
		//cout << "전체 bits: " << cntbits << " 저장 바이트수: " << (cntbits/8)+1 << endl;
		free(tempFrame);
	}
	
	fclose(fp);
}
void headerinit(header& hd, int height, int width, int QstepDC, int QstepAC, int intraPeriod)
{
	hd.intro[0] = 0; hd.intro[1] = 73; hd.intro[2] = 67; hd.intro[3] = 83; hd.intro[4] = 80;
	hd.height = height;
	hd.width  = width;
	hd.QP_DC = QstepDC;
	hd.QP_AC = QstepAC;
	hd.DPCMmode = 0;

	hd.outro = 0; // MV Pred = 0;
	for(int i=0; i<6; i++)
	{
		//(hd.outro<<=i) |= (intraPeriod>>(5-i))&1; // intraPeriod
		(hd.outro<<=1) |= (intraPeriod>>(5-i))&1; // intraPeriod		
	}

	(hd.outro <<= 1) |= 0;		// intraPrediction flag = 0;
	for(int i=0; i<6; i++)
	{
		(hd.outro <<= 1) |= 0;	// last bits = 0;
	}
}
void allintraBody(FrameData* frames, int nframes, FILE* fp)
{
	int totalblck = frames->nblocks16;
	int nblock8   = frames->nblocks8;
	int idx       = 0;
	int maxbits   = frames->splitHeight * frames->splitWidth * frames->blocks->blocksize1 * frames->blocks->blocksize1 * 8 * nframes; // Y 채널만 생각
	
	int cntbits   = 0;
	int DCbits    = 0;
	int ACbits    = 0;
	int bytbits   = 0;

	unsigned char* frame = NULL;
	unsigned char* DCResult = NULL;
	unsigned char* ACResult = NULL;
	
	frame = (unsigned char*)malloc(sizeof(unsigned char)*(maxbits/8));
	if(frame==NULL)
	{
		cout << "fail to allocate memory in allintraBody" << endl;
		exit(-1);
	}
	for(int nfrm=0; nfrm<nframes; nfrm++)
	{		
		for(int nblck16=0; nblck16<totalblck; nblck16++)
		{
			BlockData& bd = frames[nfrm].blocks[nblck16];
			// Y 16x16 블록
			for(int nblck8=0; nblck8<nblock8; nblck8++)
			{				
				DCbits = 0;
				ACbits = 0;

				(frame[cntbits++/8]<<=1) |= bd.MPMFlag[nblck8]; // mpmflag 1bit				
				(frame[cntbits++/8]<<=1) |= bd.intraPredMode[nblck8]; // intra prediction flag 1bit				
				DCResult = DCentropy(bd.intraReorderedblck8[nblck8][0], DCbits); // DC entropy result ?bits; one value
				for(int n=0; n<DCbits; n++)
					(frame[cntbits++/8]<<=1) |= DCResult[n];
				free(DCResult);
								
				(frame[cntbits++/8]<<=1) |= bd.intraACflag[nblck8]; // acflag 1bit
				
				if(bd.intraACflag[nblck8]==1)
				{
					for(int n=0; n<63; n++)
						(frame[cntbits++/8]<<=1) |= 0;	
				}
				else
				{
					ACResult = ACentropy(bd.intraReorderedblck8[nblck8], ACbits); // AC entropy result ?bits; 63 values
					for(int n=0; n<ACbits; n++)
						(frame[cntbits++/8]<<=1) |= ACResult[n];
				
					free(ACResult);
				}
			}
			// Cb Cr 8x8 블록
			CBlockData& cbbd = frames[nfrm].Cbblocks[nblck16];
			CBlockData& crbd = frames[nfrm].Crblocks[nblck16];

			DCbits = 0;
			ACbits = 0;

			DCResult = DCentropy(cbbd.intraReorderedblck[0], DCbits); // DC entropy result ?bits; one value
			for(int n=0; n<DCbits; n++)
				(frame[cntbits++/8]<<=1) |= DCResult[n];
			free(DCResult);

			(frame[cntbits++/8]<<=1) |= cbbd.intraACflag; // acflag 1bit

			if(cbbd.intraACflag==1)
			{
				for(int n=0; n<63; n++)
					(frame[cntbits++/8]<<=1) |= 0;	
			}
			else
			{
				ACResult = ACentropy(cbbd.intraReorderedblck, ACbits); // AC entropy result ?bits; 63 values
				for(int n=0; n<ACbits; n++)
					(frame[cntbits++/8]<<=1) |= ACResult[n];
			
				free(ACResult);
			}
			DCbits = 0;
			ACbits = 0;

			DCResult = DCentropy(crbd.intraReorderedblck[0], DCbits); // DC entropy result ?bits; one value
			for(int n=0; n<DCbits; n++)
				(frame[cntbits++/8]<<=1) |= DCResult[n];
			free(DCResult);

			(frame[cntbits++/8]<<=1) |= crbd.intraACflag; // acflag 1bit
			if(crbd.intraACflag==1)
			{
				for(int n=0; n<63; n++)
					(frame[cntbits++/8]<<=1) |= 0;	
			}
			else
			{
				ACResult = ACentropy(crbd.intraReorderedblck, ACbits); // AC entropy result ?bits; 63 values
				for(int n=0; n<ACbits; n++)
					(frame[cntbits++/8]<<=1) |= ACResult[n];
				free(ACResult);
			}
		}	
	}
	fwrite(frame, (cntbits/8)+1, 1, fp); 
	free(frame);
}
void intraBody(FrameData& frm, unsigned char* tempFrame, int& cntbits)
{
	int totalblck = frm.nblocks16;
	int nblock8   = frm.nblocks8;
	int idx       = 0;
	
	int DCbits    = 0;
	int ACbits    = 0;
	int bytbits   = 0;

	unsigned char* DCResult = NULL;
	unsigned char* ACResult = NULL;
	
	//cntbits = 0;
	//cout << "intra frame bits: " << cntbits << endl;
	for(int nblck16=0; nblck16<totalblck; nblck16++)
	{
		BlockData& bd = frm.blocks[nblck16];
		// Y 16x16 블록
		//cout << nblck16 << " blocks" << endl;
		for(int nblck8=0; nblck8<nblock8; nblck8++)
		{				
			DCbits = 0;
			ACbits = 0;

			(tempFrame[cntbits++/8]<<=1) |= bd.MPMFlag[nblck8];				 // mpmflag 1bit				
			(tempFrame[cntbits++/8]<<=1) |= bd.intraPredMode[nblck8];		 // intra prediction flag 1bit				

			DCResult = DCentropy(bd.intraReorderedblck8[nblck8][0], DCbits); // DC entropy result ?bits; one value
			for(int n=0; n<DCbits; n++)
				(tempFrame[cntbits++/8]<<=1) |= DCResult[n];
			free(DCResult);
				

			(tempFrame[cntbits++/8]<<=1) |= bd.intraACflag[nblck8];		  // acflag 1bit
			if(bd.intraACflag[nblck8]==1)
			{
				for(int n=0; n<63; n++)
					(tempFrame[cntbits++/8]<<=1) |= 0;
			}
			else
			{
				ACResult = ACentropy(bd.intraReorderedblck8[nblck8], ACbits); // AC entropy result ?bits; 63 values
				for(int n=0; n<ACbits; n++)
					(tempFrame[cntbits++/8]<<=1) |= ACResult[n];
				free(ACResult);
			}

		}
		// Cb Cr 8x8 블록
		CBlockData& cbbd = frm.Cbblocks[nblck16];
		CBlockData& crbd = frm.Crblocks[nblck16];

		DCbits = 0;
		ACbits = 0;

		DCResult = DCentropy(cbbd.intraReorderedblck[0], DCbits);		 // DC entropy result ?bits; one value
		for(int n=0; n<DCbits; n++)
			(tempFrame[cntbits++/8]<<=1) |= DCResult[n];
		free(DCResult);


		(tempFrame[cntbits++/8]<<=1) |= cbbd.intraACflag;	   // acflag 1bit
		if(cbbd.intraACflag==1)
		{
			for(int n=0; n<63; n++)
				(tempFrame[cntbits++/8]<<=1) |= 0;
		}
		else
		{
			ACResult = ACentropy(cbbd.intraReorderedblck, ACbits); // AC entropy result ?bits; 63 values
			for(int n=0; n<ACbits; n++)
				(tempFrame[cntbits++/8]<<=1) |= ACResult[n];
			free(ACResult);
		}

		DCbits = 0;
		ACbits = 0;

		DCResult = DCentropy(crbd.intraReorderedblck[0], DCbits); // DC entropy result ?bits; one value
		for(int n=0; n<DCbits; n++)
			(tempFrame[cntbits++/8]<<=1) |= DCResult[n];
		free(DCResult);

		(tempFrame[cntbits++/8]<<=1) |= crbd.intraACflag; // acflag 1bit
		if(crbd.intraACflag==1)
		{
			for(int n=0; n<63; n++)
				(tempFrame[cntbits++/8]<<=1) |= 0;
		}
		else
		{
			ACResult = ACentropy(crbd.intraReorderedblck, ACbits); // AC entropy result ?bits; 63 values
			for(int n=0; n<ACbits; n++)
				(tempFrame[cntbits++/8]<<=1) |= ACResult[n];
			free(ACResult);
		}

	}
}
void interBody(FrameData& frm, unsigned char* tempFrame, int& cntbits)
{
	int totalblck = frm.nblocks16;
	int nblock8   = frm.nblocks8;
	int idx       = 0;
	
	int DCbits    = 0;
	int ACbits    = 0;
	int bytbits   = 0;
	int xMVbits   = 0;
	int yMVbits   = 0;

	unsigned char* DCResult = NULL;
	unsigned char* ACResult = NULL;
	unsigned char* MVResult = NULL;
	
	for(int nblck16=0; nblck16<totalblck; nblck16++)
	{
		BlockData& bd = frm.blocks[nblck16];
		
		(tempFrame[cntbits++/8] <<= 1) |= 1;  // mv modeflag
		
		MVResult = MVentropy(bd.mv, xMVbits, yMVbits);  // mv; Reconstructedmv는 차분벡터가 보내지는게 아닌 원래 복원된 벡터를 보낸것임; 그래서 mv로 변경

		for(int n=0; n<xMVbits+yMVbits; n++)
				(tempFrame[cntbits++/8]<<=1) |= MVResult[n];
		free(MVResult);

		// Y 16x16 블록
		for(int nblck8=0; nblck8<nblock8; nblck8++)
		{	
			// dc, ac
			DCbits = 0;
			ACbits = 0;

			DCResult = DCentropy(bd.interReorderedblck8[nblck8][0], DCbits); // DC entropy result ?bits; one value
			for(int n=0; n<DCbits; n++)
				(tempFrame[cntbits++/8]<<=1) |= DCResult[n];
			free(DCResult);
				
			(tempFrame[cntbits++/8]<<=1) |= bd.interACflag[nblck8]; // acflag 1bit
			if(bd.interACflag[nblck8] == 1)
			{
				for(int n=0; n<63; n++)
					(tempFrame[cntbits++/8]<<=1) |= 0;
			}
			else
			{
				ACResult = ACentropy(bd.interReorderedblck8[nblck8], ACbits); // AC entropy result ?bits; 63 values
				for(int n=0; n<ACbits; n++)
					(tempFrame[cntbits++/8]<<=1) |= ACResult[n];
				free(ACResult);
			}
		}
		//cout << "Yframe bits: " << cntbits << endl;

		// Cb Cr 8x8 블록
		CBlockData& cbbd = frm.Cbblocks[nblck16];
		CBlockData& crbd = frm.Crblocks[nblck16];

		DCbits = 0;
		ACbits = 0;

		DCResult = DCentropy(cbbd.interReorderedblck[0], DCbits); // DC entropy result ?bits; one value
		for(int n=0; n<DCbits; n++)
			(tempFrame[cntbits++/8]<<=1) |= DCResult[n];
		free(DCResult);

		(tempFrame[cntbits++/8]<<=1) |= cbbd.interACflag; // acflag 1bit
		if(cbbd.interACflag == 1)
		{
			for(int n=0; n<63; n++)
				(tempFrame[cntbits++/8]<<=1) |= 0;
		}
		else
		{
			ACResult = ACentropy(cbbd.interReorderedblck, ACbits); // AC entropy result ?bits; 63 values
			for(int n=0; n<ACbits; n++)
				(tempFrame[cntbits++/8]<<=1) |= ACResult[n];
			free(ACResult);
		}

		DCbits = 0;
		ACbits = 0;

		DCResult = DCentropy(crbd.interReorderedblck[0], DCbits); // DC entropy result ?bits; one value
		for(int n=0; n<DCbits; n++)
			(tempFrame[cntbits++/8]<<=1) |= DCResult[n];
		free(DCResult);

		(tempFrame[cntbits++/8]<<=1) |= crbd.interACflag; // acflag 1bit
		if(crbd.interACflag == 1)
		{
			for(int n=0; n<63; n++)
				(tempFrame[cntbits++/8]<<=1) |= 0;
		}
		else
		{
			ACResult = ACentropy(crbd.interReorderedblck, ACbits); // AC entropy result ?bits; 63 values
			for(int n=0; n<ACbits; n++)
				(tempFrame[cntbits++/8]<<=1) |= ACResult[n];
			free(ACResult);
		}
	}
}
int DCentropy(int DCval, unsigned char *DCentropyResult)
{
	int nbits = 0;
	int value = 0;
	int sign  = 0;
	int exp   = 0;
	int c     = 0;
	int idx   = 0;

	sign  = (DCval>=0)?1:0;
	value = abs(DCval);

	if(value==0)						 nbits=2;
	else if(value == 1)					 nbits=4;
	else if(value>=2    &&  value<=3)    nbits=5;
	else if(value>=4    &&  value<=7)    nbits=6;
	else if(value>=8    &&  value<=15)   nbits=7;
	else if(value>=16   &&  value<=31)   nbits=8;
	else if(value>=32   &&  value<=63)   nbits=10;
	else if(value>=64   &&  value<=127)  nbits=12;
	else if(value>=128  &&  value<=255)  nbits=14;
	else if(value>=256  &&  value<=511)  nbits=16;
	else if(value>=512  &&  value<=1023) nbits=18;
	else if(value>=1024 &&  value<=2047) nbits=20;
	else if(value>=2048)				 nbits=22;

	DCentropyResult = (unsigned char*)malloc(sizeof(unsigned char)*nbits);	
	if(DCentropyResult==NULL)
	{
		cout << "fail to allocate DCentropyResult" << endl;
		exit(-1);
	}
	
	if(value == 0)					  
	{
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=0;
	}
	else if(value == 1)
	{
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;
	}
	else if(value>=2   &&  value<=3)
	{
		exp=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=sign;
		c=value-2;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;

	}
	else if(value>=4   &&  value<=7)
	{
		exp=2;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-4;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=8   &&  value<=15)
	{
		exp=3;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=sign;

		c=value-8;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=16  &&  value<=31)
	{
		exp=4;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-16;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=32  &&  value<=63)
	{
		exp=5;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-32;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=64  &&  value<=127)
	{
		exp=6;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-64;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=128 &&  value<=255)
	{
		exp=7;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-128;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=256 &&  value<=511)
	{
		exp=8;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-256;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=512 &&  value<=1023)
	{
		exp=9;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-512;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=1024 &&  value<=2047)
	{
		exp=10;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-1024;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=2048)
	{
		exp=11;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-2048;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}

	return nbits;
}
unsigned char* DCentropy(int DCval, int& nbits)
{
	int value = 0;
	int sign  = 0;
	int exp   = 0;
	int c     = 0;
	int idx   = 0;

	sign  = (DCval>=0)?1:0;
	value = abs(DCval);

	if(value==0)						 nbits=2;
	else if(value == 1)					 nbits=4;
	else if(value>=2    &&  value<=3)    nbits=5;
	else if(value>=4    &&  value<=7)    nbits=6;
	else if(value>=8    &&  value<=15)   nbits=7;
	else if(value>=16   &&  value<=31)   nbits=8;
	else if(value>=32   &&  value<=63)   nbits=10;
	else if(value>=64   &&  value<=127)  nbits=12;
	else if(value>=128  &&  value<=255)  nbits=14;
	else if(value>=256  &&  value<=511)  nbits=16;
	else if(value>=512  &&  value<=1023) nbits=18;
	else if(value>=1024 &&  value<=2047) nbits=20;
	else if(value>=2048)				 nbits=22;

	unsigned char* DCentropyResult = (unsigned char*)malloc(sizeof(unsigned char)*nbits);	
	
	if(DCentropyResult==NULL)
	{
		cout << "fail to allocate DCentropyResult" << endl;
		exit(-1);
	}
	
	if(value == 0)					  
	{
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=0;
	}
	else if(value == 1)
	{
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;
	}
	else if(value>=2   &&  value<=3)
	{
		exp=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=sign;
		c=value-2;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;

	}
	else if(value>=4   &&  value<=7)
	{
		exp=2;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-4;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=8   &&  value<=15)
	{
		exp=3;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=sign;

		c=value-8;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=16  &&  value<=31)
	{
		exp=4;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-16;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=32  &&  value<=63)
	{
		exp=5;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-32;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=64  &&  value<=127)
	{
		exp=6;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-64;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=128 &&  value<=255)
	{
		exp=7;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-128;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=256 &&  value<=511)
	{
		exp=8;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-256;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=512 &&  value<=1023)
	{
		exp=9;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-512;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=1024 &&  value<=2047)
	{
		exp=10;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-1024;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(value>=2048)
	{
		exp=11;
		for(int n=0; n<exp-2; n++)
			DCentropyResult[idx++]=1;
		DCentropyResult[idx++]=0;
		DCentropyResult[idx++]=sign;

		c=value-2048;
		for(int n=exp; n>0; n--)
			DCentropyResult[idx++] = (c>>(n-1))&1;
	}

	/*cout << "DC entropy" << endl;
	for(int i=0; i<idx; i++)
	{
		cout << (int)DCentropyResult[i] << " ";
	}
	cout << endl;*/
	return DCentropyResult;
}
int ACentropy(int* reordblck, unsigned char *ACentropyResult)
{
	int nbits  = 0;
	int value  = 0;
	int sign   = 0;
	int exp   = 0;
	int c     = 0;
	int idx   = 0;
	int length = 63; // except DC value of total 64 values

	for(int i=1; i<length; i++)
	{
		value = abs(reordblck[i+1]);
		if(value==0)						 nbits+=2;
		else if(value == 1)					 nbits+=4;
		else if(value>=2    &&  value<=3)    nbits+=5;
		else if(value>=4    &&  value<=7)    nbits+=6;
		else if(value>=8    &&  value<=15)   nbits+=7;
		else if(value>=16   &&  value<=31)   nbits+=8;
		else if(value>=32   &&  value<=63)   nbits+=10;
		else if(value>=64   &&  value<=127)  nbits+=12;
		else if(value>=128  &&  value<=255)  nbits+=14;
		else if(value>=256  &&  value<=511)  nbits+=16;
		else if(value>=512  &&  value<=1023) nbits+=18;
		else if(value>=1024 &&  value<=2047) nbits+=20;
		else if(value>=2048)				 nbits+=22;
	}


	ACentropyResult = (unsigned char*) malloc(sizeof(unsigned char)*nbits);	
	if(ACentropyResult==NULL)
	{
		cout << "fail to allocate ACentropyResult" << endl;
		exit(-1);
	}

	for(int i=1; i<length; i++)
	{
		sign  = (reordblck[i+1]>=0)?1:0;
		value = abs(reordblck[i+1]);
		if(value == 0)					  
		{
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=0;
		}
		else if(value == 1)
		{
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;
		}
		else if(value>=2   &&  value<=3)
		{
			exp=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=sign;
			c=value-2;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;

		}
		else if(value>=4   &&  value<=7)
		{
			exp=2;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-4;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=8   &&  value<=15)
		{
			exp=3;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=sign;

			c=value-8;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=16  &&  value<=31)
		{
			exp=4;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-16;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=32  &&  value<=63)
		{
			exp=5;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-32;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=64  &&  value<=127)
		{
			exp=6;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-64;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=128 &&  value<=255)
		{
			exp=7;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-128;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=256 &&  value<=511)
		{
			exp=8;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-256;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=512 &&  value<=1023)
		{
			exp=9;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-512;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=1024 &&  value<=2047)
		{
			exp=10;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-1024;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=2048)
		{
			exp=11;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-2048;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
	}

	return nbits;
}
unsigned char* ACentropy(int* reordblck, int& nbits)
{
	int value  = 0;
	int sign   = 0;
	int exp    = 0;
	int c      = 0;
	int idx    = 0;
	int length = 63; // except DC value of total 64 values

	for(int i=0; i<length; i++)
	{
		value = abs(reordblck[i+1]);
		if(value==0)						 nbits+=2;
		else if(value == 1)					 nbits+=4;
		else if(value>=2    &&  value<=3)    nbits+=5;
		else if(value>=4    &&  value<=7)    nbits+=6;
		else if(value>=8    &&  value<=15)   nbits+=7;
		else if(value>=16   &&  value<=31)   nbits+=8;
		else if(value>=32   &&  value<=63)   nbits+=10;
		else if(value>=64   &&  value<=127)  nbits+=12;
		else if(value>=128  &&  value<=255)  nbits+=14;
		else if(value>=256  &&  value<=511)  nbits+=16;
		else if(value>=512  &&  value<=1023) nbits+=18;
		else if(value>=1024 &&  value<=2047) nbits+=20;
		else if(value>=2048)				 nbits+=22;
	}


	unsigned char* ACentropyResult = (unsigned char*) malloc(sizeof(unsigned char)*nbits);	
	if(ACentropyResult==NULL)
	{
		cout << "fail to allocate ACentropyResult" << endl;
		exit(-1);
	}

	//cout << "AC entropy" << endl;
	for(int i=0; i<length; i++)
	{
		sign  = (reordblck[i+1]>=0)?1:0;
		value = abs(reordblck[i+1]);

		//////////
		int previdx = idx;
		//////////

		if(value == 0)					  
		{
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=0;
		}
		else if(value == 1)
		{
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;
		}
		else if(value>=2   &&  value<=3)
		{
			exp=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=sign;
			c=value-2;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;

		}
		else if(value>=4   &&  value<=7)
		{
			exp=2;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-4;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=8   &&  value<=15)
		{
			exp=3;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=sign;

			c=value-8;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=16  &&  value<=31)
		{
			exp=4;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-16;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=32  &&  value<=63)
		{
			exp=5;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-32;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=64  &&  value<=127)
		{
			exp=6;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-64;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=128 &&  value<=255)
		{
			exp=7;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-128;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=256 &&  value<=511)
		{
			exp=8;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-256;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=512 &&  value<=1023)
		{
			exp=9;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-512;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=1024 &&  value<=2047)
		{
			exp=10;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-1024;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}
		else if(value>=2048)
		{
			exp=11;
			for(int n=0; n<exp-2; n++)
				ACentropyResult[idx++]=1;
			ACentropyResult[idx++]=0;
			ACentropyResult[idx++]=sign;

			c=value-2048;
			for(int n=exp; n>0; n--)
				ACentropyResult[idx++] = (c>>(n-1))&1;
		}

		/*for(int nb=previdx; nb<idx; nb++)
		{
			cout << (int)ACentropyResult[nb] << " ";
		}
		cout << endl;*/
	}
	//system("pause");
	return ACentropyResult;
}
unsigned char* MVentropy(MotionVector mv, int& nbitsx, int& nbitsy)
{
	int xValue = 0;
	int yValue = 0;
	int xsign  = 0;
	int ysign  = 0;
	int exp    = 0;
	int c      = 0;
	int idx    = 0;

	xsign  = (mv.x>=0)?1:0;
	xValue = abs(mv.x);

	if(xValue==0)						   nbitsx=2;
	else if(xValue==1)					   nbitsx=4;
	else if(xValue>=2    &&  xValue<=3)    nbitsx=5;
	else if(xValue>=4    &&  xValue<=7)    nbitsx=6;
	else if(xValue>=8    &&  xValue<=15)   nbitsx=7;
	else if(xValue>=16   &&  xValue<=31)   nbitsx=8;
	else if(xValue>=32   &&  xValue<=63)   nbitsx=10;
	else if(xValue>=64   &&  xValue<=127)  nbitsx=12;
	else if(xValue>=128  &&  xValue<=255)  nbitsx=14;
	else if(xValue>=256  &&  xValue<=511)  nbitsx=16;
	else if(xValue>=512  &&  xValue<=1023) nbitsx=18;
	else if(xValue>=1024 &&  xValue<=2047) nbitsx=20;
	else if(xValue>=2048)				   nbitsx=22;	

	ysign  = (mv.y>=0)?1:0;
	yValue = abs(mv.y);

	if(yValue==0)						   nbitsy=2;
	else if(yValue==1)					   nbitsy=4;
	else if(yValue>=2    &&  yValue<=3)    nbitsy=5;
	else if(yValue>=4    &&  yValue<=7)    nbitsy=6;
	else if(yValue>=8    &&  yValue<=15)   nbitsy=7;
	else if(yValue>=16   &&  yValue<=31)   nbitsy=8;
	else if(yValue>=32   &&  yValue<=63)   nbitsy=10;
	else if(yValue>=64   &&  yValue<=127)  nbitsy=12;
	else if(yValue>=128  &&  yValue<=255)  nbitsy=14;
	else if(yValue>=256  &&  yValue<=511)  nbitsy=16;
	else if(yValue>=512  &&  yValue<=1023) nbitsy=18;
	else if(yValue>=1024 &&  yValue<=2047) nbitsy=20;
	else if(yValue>=2048)				   nbitsy=22;	
	
	unsigned char* MVentropyResult = (unsigned char*)malloc(sizeof(unsigned char)*(nbitsx+nbitsy));
	if(MVentropyResult==NULL)
	{
		cout << "fail to allocate MVentropyResult" << endl;
		exit(-1);
	}
	
	if(xValue == 0)					  
	{
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=0;
	}
	else if(xValue == 1)
	{
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=xsign;
	}
	else if(xValue>=2   &&  xValue<=3)
	{
		exp=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=xsign;
		c=xValue-2;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;

	}
	else if(xValue>=4   &&  xValue<=7)
	{
		exp=2;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=xsign;

		c=xValue-4;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(xValue>=8   &&  xValue<=15)
	{
		exp=3;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=xsign;

		c=xValue-8;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(xValue>=16  &&  xValue<=31)
	{
		exp=4;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=xsign;

		c=xValue-16;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(xValue>=32  &&  xValue<=63)
	{
		exp=5;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=xsign;

		c=xValue-32;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(xValue>=64  &&  xValue<=127)
	{
		exp=6;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=xsign;

		c=xValue-64;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(xValue>=128 &&  xValue<=255)
	{
		exp=7;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=xsign;

		c=xValue-128;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(xValue>=256 &&  xValue<=511)
	{
		exp=8;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=xsign;

		c=xValue-256;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(xValue>=512 &&  xValue<=1023)
	{
		exp=9;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=xsign;

		c=xValue-512;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(xValue>=1024 &&  xValue<=2047)
	{
		exp=10;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=xsign;

		c=xValue-1024;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(xValue>=2048)
	{
		exp=11;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=xsign;

		c=xValue-2048;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	


	if(yValue == 0)					  
	{
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=0;
	}
	else if(yValue == 1)
	{
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=ysign;
	}
	else if(yValue>=2   &&  yValue<=3)
	{
		exp=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=ysign;
		c=yValue-2;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;

	}
	else if(yValue>=4   &&  yValue<=7)
	{
		exp=2;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=ysign;

		c=yValue-4;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(yValue>=8   &&  yValue<=15)
	{
		exp=3;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=ysign;

		c=yValue-8;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(yValue>=16  &&  yValue<=31)
	{
		exp=4;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=ysign;

		c=yValue-16;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(yValue>=32  &&  yValue<=63)
	{
		exp=5;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=ysign;

		c=yValue-32;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(yValue>=64  &&  yValue<=127)
	{
		exp=6;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=ysign;

		c=yValue-64;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(yValue>=128 &&  yValue<=255)
	{
		exp=7;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=ysign;

		c=yValue-128;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(yValue>=256 &&  yValue<=511)
	{
		exp=8;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=ysign;

		c=yValue-256;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(yValue>=512 &&  yValue<=1023)
	{
		exp=9;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=ysign;

		c=yValue-512;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(yValue>=1024 &&  yValue<=2047)
	{
		exp=10;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=ysign;

		c=yValue-1024;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}
	else if(yValue>=2048)
	{
		exp=11;
		for(int n=0; n<exp-2; n++)
			MVentropyResult[idx++]=1;
		MVentropyResult[idx++]=0;
		MVentropyResult[idx++]=ysign;

		c=yValue-2048;
		for(int n=exp; n>0; n--)
			MVentropyResult[idx++] = (c>>(n-1))&1;
	}

	return MVentropyResult;
}


/* checking image function */
void checkResultY(unsigned char *Y, int width, int height)
{
	FILE* output_fp;
	char CIF_path[256] = "..\\CIF(352x288)";
	char output_name[256] = "check_test_gray.yuv";
	char output_fname[256];

	sprintf(output_fname, "%s\\%s", CIF_path, output_name);

	output_fp = fopen(output_fname, "wb");
	if(output_fp==NULL)
	{
		cout << "fail to save yuv" << endl;
		return;
	}

	fwrite(Y, sizeof(unsigned char)*height*width, 1, output_fp);
	fclose(output_fp);
}
void checkResultYUV(unsigned char *Y, unsigned char *Cb, unsigned char *Cr, int width, int height)
{
	FILE* output_fp;
	char CIF_path[256] = "..\\CIF(352x288)";
	char output_name[256] = "check_test_YUV.yuv";
	char output_fname[256];
	sprintf(output_fname, "%s\\%s", CIF_path, output_name);

	output_fp = fopen(output_fname, "wb");	
	if(output_fp==NULL)
	{
		cout << "fail to save yuv" << endl;
		return;
	}
	fwrite(Y,  sizeof(unsigned char)*height*width, 1, output_fp);
	fwrite(Cb, sizeof(unsigned char)*(height/2)*(width/2), 1, output_fp);
	fwrite(Cr, sizeof(unsigned char)*(height/2)*(width/2), 1, output_fp);
	fclose(output_fp);
}
void checkResultFrames(FrameData* frm, int width, int height, int nframe, int predtype, int chtype)
{
	FILE* output_fp;
	char CIF_path[256] = "data";

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
			fwrite(frm[i].reconstructedY, sizeof(unsigned char)*height*width, 1, output_fp);
		}
	}
	else if(chtype==SAVE_YUV)
	{
		for(int i=0; i<nframe; i++)
		{
			fwrite(frm[i].reconstructedY,  sizeof(unsigned char)*height*width, 1, output_fp);
			fwrite(frm[i].reconstructedCb,  sizeof(unsigned char)*(height/2)*(width/2), 1, output_fp);
			fwrite(frm[i].reconstructedCr,  sizeof(unsigned char)*(height/2)*(width/2), 1, output_fp);
		}
	}
	fclose(output_fp);

	for(int i=0; i<nframe; i++)
	{
		free(frm[i].reconstructedY);
		free(frm[i].reconstructedCb);
		free(frm[i].reconstructedCr);
	}
}
void checkResultRestructedFrames(FrameData* frm, int width, int height, int nframe, int type)
{
	FILE* output_fp;
	char CIF_path[256] = "..\\CIF(352x288)";
	char output_name[256] = "test_restructed_frames.yuv";
	char output_fname[256];
	sprintf(output_fname, "%s\\%s", CIF_path, output_name);

	output_fp = fopen(output_fname, "wb");
	if(output_fp==NULL)
	{
		cout << "fail to save yuv" << endl;
		return;
	}


	int totalblck = frm->nblocks16;
	int nblck8 = frm->nblocks8;
	int splitWidth = frm->splitWidth;
	int splitHeight = frm->splitHeight;
	int blocksize1 = frm->blocks->blocksize1;
	int blocksize2 = frm->blocks->blocksize2;

	unsigned char* img = (unsigned char*) calloc(width * height, sizeof(unsigned char));


	free(img);
	fclose(output_fp);

}
void checkRestructed(FrameData* frms, int nframes, int width, int height, int predtype, int chtype) // type: 1 -> Y, 3 -> YCbCr
{
	FILE* fp;
	char CIF_path[256] = "..\\CIF(352x288)";
	char output_name[256];
	if (chtype == 1) sprintf(output_name, "Test_Restructed_Ychannel.yuv");
	else if (chtype == 3) sprintf(output_name, "Test_Restructed_YCbCr.yuv");

	char output_fname[256];
	sprintf(output_fname, "%s\\%s", CIF_path, output_name);
	fp = fopen(output_fname, "wb");

	int blocksize1 = frms->blocks->blocksize1;
	int blocksize2 = frms->blocks->blocksize2;
	int totalblck = frms->nblocks16;
	int nblck8 = frms->nblocks8;
	int splitWidth = frms->splitWidth;
	int splitHeight = frms->splitHeight;
	int CbCrSplitWidth = frms->CbCrSplitWidth;
	int CbCrSplitHeight = frms->CbCrSplitHeight;
	int CbCrWidth = frms->CbCrWidth;
	int CbCrHeight = frms->CbCrHeight;


	unsigned char* Ychannel = (unsigned char *)calloc(width * height, sizeof(unsigned char));
	unsigned char* Cbchannel = (unsigned char *)calloc((width / 2) * (height / 2), sizeof(unsigned char));
	unsigned char* Crchannel = (unsigned char *)calloc((width / 2) * (height / 2), sizeof(unsigned char));

	int temp = 0;
	int tempCb = 0;
	int tempCr = 0;
	int nblck = 0;
	if (chtype == 1)
	{
		for (int nfrm = 0; nfrm < nframes; nfrm++)
		{
			FrameData& frm = frms[nfrm]; // frame
			nblck = 0;
			for (int y_interval = 0; y_interval < splitHeight; y_interval++)
			{
				for (int x_interval = 0; x_interval < splitWidth; x_interval++)
				{
					BlockData &bd = frm.blocks[nblck]; // nblck++

					for (int y = 0; y < blocksize1; y++)
					{
						for (int x = 0; x < blocksize1; x++)
						{
							Ychannel[(y_interval*blocksize1*width) + (y*width) + (x_interval*blocksize1) + x] = bd.intraRestructedblck16.block[y][x];
						}
					}
					nblck++;
				}
			}
			fwrite(Ychannel, width*height, sizeof(unsigned char), fp);
		}
	}
	else if (chtype == 3)
	{
		for (int nfrm = 0; nfrm < nframes; nfrm++)
		{
			FrameData& frm = frms[nfrm]; // frame
			nblck = 0;
			for (int y_interval = 0; y_interval < splitHeight; y_interval++)
			{
				for (int x_interval = 0; x_interval < splitWidth; x_interval++)
				{
					BlockData &bd = frm.blocks[nblck]; // nblck++

					for (int y = 0; y < blocksize1; y++)
					{
						for (int x = 0; x < blocksize1; x++)
						{
							Ychannel[(y_interval*blocksize1*width) + (y*width) + (x_interval*blocksize1) + x] = bd.intraRestructedblck16.block[y][x];
						}
					}
					nblck++;
				}
			}
			nblck = 0;
			for (int y_interval = 0; y_interval < CbCrSplitHeight; y_interval++)
			{
				for (int x_interval = 0; x_interval < CbCrSplitWidth; x_interval++)
				{
					CBlockData &cbbd = frm.Cbblocks[nblck]; // nblck++
					CBlockData &crbd = frm.Crblocks[nblck];
					int index = 0;
					for (int y = 0; y < blocksize2; y++)
					{
						for (int x = 0; x < blocksize2; x++)
						{
							tempCb = (cbbd.intraInverseDCTblck->block[y][x] > 255) ? 255 : cbbd.intraInverseDCTblck->block[y][x];
							tempCb = (tempCb < 0) ? 0 : tempCb;
							index = (y_interval*blocksize2*CbCrWidth) + (y*CbCrWidth) + (x_interval*blocksize2) + x;
							Cbchannel[index] = tempCb;

							tempCr = (crbd.intraInverseDCTblck->block[y][x] > 255) ? 255 : crbd.intraInverseDCTblck->block[y][x];
							tempCr = (tempCr < 0) ? 0 : tempCr;
							Crchannel[index] = tempCr;
						}
					}
					nblck++;
				}
			}
			fwrite(Ychannel, width*height, sizeof(unsigned char), fp);
			fwrite(Cbchannel, sizeof(unsigned char), (width / 2)*(height / 2), fp);
			fwrite(Crchannel, sizeof(unsigned char), (width / 2)*(height / 2), fp);
		}
	}

	free(Ychannel);
	free(Cbchannel);
	free(Crchannel);

	fclose(fp);
}