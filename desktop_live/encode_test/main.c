#include "capture.h"
#include "log.h"
#include "encode.h"
#include <stdio.h>
#include <stdlib.h>
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#pragma comment(lib, "capture.lib")
#pragma comment(lib, "log.lib")
#pragma comment(lib, "encode.lib")
#pragma comment(lib, "winmm.lib")

int main()
{
	int ret = -1;
	LOG *log;
	char log_str[1024];
	uint8_t *data = NULL;
	char *config_file = "E:\\desktop_live\\desktop_live\\lib\\config.ini";
	char *log_file = "D:\\desktop_live_log.txt";
	char *record_file = "D:\\desktop_live.mp4";
	unsigned long size = 0;
	int times = 0;
	int log_level;
	int out_way;
	int width = 0;
	int height = 0;
	char *dest = NULL;
	unsigned long dest_size = 0;
	long long pts = 0;
	long long dts = 0;
	DWORD start_time, end_time;
	AUDIO_PACKET ap[30] = {0};
	int ap_len = 0;
	int i = 0;
	log_level = GetPrivateProfileIntA("log", 
		"level", 0, config_file);
	out_way = GetPrivateProfileIntA("log", 
		"out_way", 0, config_file);
	log = init_log(log_file , log_level, out_way); 
	if (log == NULL)
	{
		ret = -1;
		return ret;
	}

	ret = start_capture(log, config_file);
	if (0 != ret)
	{
		printf("start capture failed\n");
		return -1;
	}

	ret = init_ercoder(log, config_file, record_file, 1);
	if (ret != 0)
	{
		printf("init encoder failed\n");
		return -2;
	}
	start_time = end_time = timeGetTime();
	while(10*1000 > (end_time - start_time))
	{
		if (0 == get_video_frame(&data, &size, &width, &height))
		{
			ret = encode_video(data, width, height, &dest, &dest_size, &pts, &dts);
			if (ret != 0)
			{
				printf("encode failed : %d\n",ret);
			}
			else
			{
				free(dest);
			}

			times++;
			printf("count = %d\n", times);
			free(data);
		}

		if (0 == get_audio_frame(&data, &size))
		{
			ap_len = 0;
			ret = encode_audio(data, size, ap, &ap_len);
			if (ret != 0)
			{
				printf("encode audio failed\n");
			}
			else
			{
				for (i=0; i<ap_len; i++)
				{
					free(ap[i].data);
				}
			}
			
//			printf("audio data size = %d\n", size);
			free(data);
		}
		end_time = timeGetTime();
	}

	stop_capture();

	free_capture();

	fflush_encoder();

	free_encoder();

	free_log();

	_CrtDumpMemoryLeaks();
	return 0;
}