#include "capture.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#pragma comment(lib, "capture.lib")
#pragma comment(lib, "log.lib")

int main(int argc, char **argv)
{
	int ret = -1;
	uint8_t *data = NULL;
	unsigned long size = 0;
	int times = 0;
	int width = 0;
	int height = 0;
	CAPTURECONFIG captureConfig;
	PCAPTURECONFIG pCaptureConfig = &captureConfig;
	pCaptureConfig->fps = 10;
	pCaptureConfig->channels = 2;
	pCaptureConfig->bits_per_sample = 16;
	pCaptureConfig->samples_per_sec = 48000;
	pCaptureConfig->avg_bytes_per_sec = 48000;

	InitLog(LOG_INFO, OUT_FILE);

	ret = InitCapture(pCaptureConfig);
	if (SECCESS != ret)
	{
		printf("start capture failed\n");
		return -1;
	}

	ret = StartCapture();
	if (SECCESS != ret)
	{
		printf("start capture failed\n");
		return -1;
	}
/*
		StopCapture();

		ret = StartCapture();
		if (SECCESS != ret)
		{
			printf("start capture failed\n");
			return -1;
		}
*/
	while(times < 100)
	{
		if (SECCESS == GetVideoFrame(&data, &size, &width, &height))
		{
			times++;
			printf("video data size = %d\n", size);
			free(data);
		}

		if (SECCESS == GetAudioFrame(&data, &size))
		{
			printf("audio data size = %d\n", size);
			free(data);
		}
	}
	

	StopCapture();
	
	FreeCapture();

	FreeLog();
	_CrtDumpMemoryLeaks();
	return 0;
}