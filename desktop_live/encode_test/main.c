#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include "capture.h"
#include "log.h"
#include "encode.h"

#pragma comment(lib, "capture.lib")
#pragma comment(lib, "log.lib")
#pragma comment(lib, "encode.lib")
#pragma comment(lib, "winmm.lib")

int main(int argc, char **argv)
{
	int ret = -1;
	uint8_t *data = NULL;
	unsigned long size = 0;
	int times = 0;
	int width = 0;
	int height = 0;
	char *dest = NULL;
	unsigned long dest_size = 0;
	long long pts = 0;
	long long dts = 0;
	AUDIOPACKET ap[30] = {0};
	int ap_len = 0;
	int i = 0;
	CAPTURECONFIG captureConfig;
	PCAPTURECONFIG pCaptureConfig = &captureConfig;
	ENCODECONFIG encodeConfig;
	PENCODECONFIG pEncodeConfig = &encodeConfig;
	PENCODER pEncoder;
	PCAPTURE pCapture;
	pCaptureConfig->fps = 6;
	pCaptureConfig->channels = 2;
	pCaptureConfig->bits_per_sample = 16;
	pCaptureConfig->samples_per_sec = 48000;
	pCaptureConfig->avg_bytes_per_sec = 48000;

	pEncodeConfig->fps = 6;
	pEncodeConfig->width = 720;
	pEncodeConfig->height = 480;
	pEncodeConfig->bit_rate = 400000;
	pEncodeConfig->channels = 2;
	pEncodeConfig->bits_per_sample = 16;
	pEncodeConfig->sample_rate = 48000;
	pEncodeConfig->avg_bytes_per_sec = 48000;
	pEncodeConfig->record = 1;

	memcpy(pEncodeConfig->record_file, "D:\\desktop_live.mp4", 20);

	InitLog(LOG_DEBUG, OUT_FILE);

	pCapture = InitCapture(pCaptureConfig);
	if (NULL == pCapture)
	{
		printf("init capture failed\n");
		return -1;
	}

	pEncoder = InitEncoder(pEncodeConfig);
	if (NULL == pEncoder)
	{
		printf("init encoder failed\n");
		return -1;
	}

	ret = StartCapture(pCapture);
	if (SECCESS != ret)
	{
		printf("start capture failed\n");
		return -1;
	}

	while(times < 100)
	{
		if (SECCESS == GetVideoFrame(pCapture, &data, &size, &width, &height))
		{
			ret = EncodeVideo(pEncoder, data, width, height, &dest, &dest_size, &pts, &dts);
			if (ret == SECCESS)
			{
				free(dest);
			}

			times++;
			printf("video data size = %d\n", size);
			free(data);
		}

		if (SECCESS == GetAudioFrame(pCapture, &data, &size))
		{
			ap_len = 0;
			ret = EncodeAudio(pEncoder, data, size, ap, &ap_len);
			if (ret == SECCESS)
			{
				for (i=0; i<ap_len; i++)
				{
					free(ap[i].data);
				}
			}

			printf("audio data size = %d\n", size);
			free(data);
		}
	}

	StopCapture(pCapture);
	FreeCapture(pCapture);

	FflushEncoder(pEncoder);
	FreeEncoder(pEncoder);

	FreeLog();
	_CrtDumpMemoryLeaks();
	return 0;
}