//------------------------------------------------------------------------------
//
//	Copyright (C) 2019 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		: Simple encoder example
//	File		: main.cpp
//	Description	: This program is a simple example program that is encoded 
//				   from camera and saved as a file.
//	Author		: SeongO Park ( ray@nexell.co.kr )
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>		// for clock()

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <linux/media-bus-format.h>

#include <nx-v4l2.h>

#include <nx_video_alloc.h>
#include <nx_video_api.h>


#define	MAX_BUFFER_COUNT	(4)
#define CAMERA_DEVICE		"/dev/video6"
#define OUT_FILE			"/home/root/enc_out.264"


int main( int argc, const char *argv[] )
{
	
	//	camera informations
	int32_t module = 0;
	int32_t pixelFormat = V4L2_PIX_FMT_YUV420;
	int32_t busFormat = MEDIA_BUS_FMT_YUYV8_2X8;
	int32_t width = 1280;
	int32_t height = 720;
	int32_t videoDevice = nx_clipper_video;
	
	int		cameraFd;
	int32_t	bufIdx = -1;

	int32_t	size;
	int		ret;
	
	NX_VID_MEMORY_HANDLE hCamMem[MAX_BUFFER_COUNT] = {0, };
	

	//	open camera device
	cameraFd = open(CAMERA_DEVICE, O_RDWR);
	
	printf( "cameraFd = %d\n", cameraFd );

	//	open & initialize camera
	nx_v4l2_set_format(cameraFd, videoDevice, width, height, pixelFormat);
	if( 0 != ret )
	{
		printf("nx_v4l2_set_format failed !!!\n");
		exit(-1);
	}
	
	ret = nx_v4l2_reqbuf(cameraFd, videoDevice, MAX_BUFFER_COUNT);
	if (ret) {
		printf("nx_v4l2_reqbuf failed !!\n");
		exit(-1);
	}


	//	allocate & queue buffer
	for ( int i=0; i<MAX_BUFFER_COUNT ; i++ )
	{
		hCamMem[i] = NX_AllocateVideoMemory( width , height, 3, V4L2_PIX_FMT_YUV420, 4096 );
		if( hCamMem[i] == NULL)
		{
			printf("NX_AllocateVideoMemory Error\n");
			exit (-1);
		}
		//NX_MapVideoMemory(hCamMem[i]);

		size = hCamMem[i]->size[0] + hCamMem[i]->size[1] + hCamMem[i]->size[2];
		ret = nx_v4l2_qbuf(cameraFd, videoDevice, 1, i, &hCamMem[i]->sharedFd[0], &size);
		if( 0 != ret )
		{
			printf("nx_v4l2_qbuf failed !!!\n");
			exit(-1);
		}
	}


	//	Encoder
	NX_V4L2ENC_HANDLE hEnc;
	NX_V4L2ENC_PARA encPara;
	uint8_t *pSeqData;
	int32_t seqSize;
	uint32_t frmCnt = 0;
	
	//	FILE
	clock_t startClock, endClock, clockSum;
	FILE *hFile = fopen(OUT_FILE, "wb");

	hEnc = NX_V4l2EncOpen(V4L2_PIX_FMT_H264);
	memset(&encPara, 0, sizeof(encPara));
	encPara.width = width;
	encPara.height = height;
	encPara.fpsNum = 30;
	encPara.fpsDen = 1;
	encPara.keyFrmInterval = 30;
	encPara.bitrate = 8 * 1024 * 1024;	// 8Mbps
	encPara.maximumQp = 0;
	encPara.rcVbvSize = 0;
	encPara.disableSkip = 0;
	encPara.RCDelay = 0;
	encPara.gammaFactor = 0;
	encPara.initialQp = 25;
	encPara.numIntraRefreshMbs = 0;
	encPara.searchRange = 0;
	encPara.enableAUDelimiter = 0;
	encPara.imgFormat = hCamMem[0]->format;
	encPara.imgBufferNum = MAX_BUFFER_COUNT;
	encPara.imgPlaneNum = hCamMem[0]->planes;
	encPara.pImage = hCamMem[0];
	
	ret = NX_V4l2EncInit(hEnc, &encPara);
	if (ret < 0)
	{
		printf("video encoder initialization is failed!!!\n");
		exit(-1);
	}

	ret = NX_V4l2EncGetSeqInfo(hEnc, &pSeqData, &seqSize);
	if (ret < 0)
	{
		printf("Getting Sequence header is failed!!!\n");
		exit(-1);
	}

	if( hFile )
	{
		fwrite( pSeqData, 1, seqSize, hFile );
	}

	printf( "seqSize = %d\n", seqSize );

	nx_v4l2_streamon(cameraFd, videoDevice);

	if( 0 != ret )
	{
		printf("nx_v4l2_streamon failed !!!\n");
		exit(-1);
	}


	while( 1 )
	{
		NX_V4L2ENC_IN encIn;
		NX_V4L2ENC_OUT encOut;

		nx_v4l2_dqbuf(cameraFd, videoDevice, 1, &bufIdx);

		startClock = clock();
		memset(&encIn, 0, sizeof(NX_V4L2ENC_IN));
		memset(&encOut, 0, sizeof(NX_V4L2ENC_OUT));

		encIn.pImage = hCamMem[bufIdx];
		encIn.imgIndex = bufIdx;
		encIn.forcedIFrame = 0;
		encIn.forcedSkipFrame = 0;
		encIn.quantParam = 25;

		ret = NX_V4l2EncEncodeFrame(hEnc, &encIn, &encOut);
		if (ret < 0)
		{
			printf("[%d] Frame]NX_V4l2EncEncodeFrame() is failed!!\n", frmCnt);
			break;
		}
		endClock = clock();
		clockSum += (endClock - startClock);

		if( hFile )
		{
			fwrite( encOut.strmBuf, 1, encOut.strmSize, hFile );
		}

		printf("[%04d Frm]Size = %5d, Type = %1d\n", frmCnt, encOut.strmSize, encOut.frameType);

		nx_v4l2_qbuf(cameraFd, videoDevice, 1, bufIdx, &hCamMem[bufIdx]->sharedFd[0], &size);
		frmCnt ++;
		
		if( frmCnt == 100 )
		{
			printf("Writing Done!!!\n");
			printf("Total frame = %d, Total Time = %ld msec, avg fps = %.2f fps\n", frmCnt, clockSum, ((float)(frmCnt * CLOCKS_PER_SEC)) / ((float) clockSum ) );
			fclose( hFile );
			hFile = 0;
			break;
		}
	}

	return 0;
}
