#include "capture.h"
#include "log.h"
#include "encode.h"
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "capture.lib")
#pragma comment(lib, "log.lib")
#pragma comment(lib, "encode.lib")
#pragma comment(lib, "Winmm.lib")

int main(int argc, char **argv)
{
	int ret = -1;
	LOG *log;
	char log_str[1024];
	uint8_t *data = NULL;
	char *config_file = "E:\\desktop_live\\desktop_live\\lib\\config.ini";
	char *log_file = "D:\\desktop_live_log.txt";
	char *recore_file = "D:\\desktop_live.mp4";
	int record = 1;
	unsigned long size = 0;
	long pts = 0;
	long dts = 0;
	int log_level;
	int out_way;
	DWORD start_time = 0, end_time = 0;
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

	ret = start_encode(log, config_file, recore_file, record);
	if (0 != ret)
	{
		printf("start encode failed\n");
		return -1;
	}

	ret = start_capture(log, config_file);
	if (0 != ret)
	{
		printf("start capture failed\n");
		return -1;
	}
	start_time = end_time = timeGetTime();
	while(end_time - start_time < 10*1000)
	{
		if (0 == get_video_packet(&data, &size, &pts, &dts))
		{
			printf("video data size = %d\n", size);
			free(data);
		}

		if (0 == get_audio_packet(&data, &size, &pts, &dts))
		{
			printf("audio data size = %d\n", size);
			free(data);
		}
		end_time = timeGetTime();
	}

	stop_encode();

	stop_capture();

	free_log();

	return 0;
}