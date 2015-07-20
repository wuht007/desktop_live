#include "capture.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "capture.lib")
#pragma comment(lib, "log.lib")

int main(int argc, char **argv)
{
	int ret = -1;
	LOG *log;
	char log_str[1024];
	uint8_t *data = NULL;
	char *config_file = "E:\\desktop_live\\desktop_live\\lib\\config.ini";
	char *log_file = "D:\\desktop_live_log.txt";
	unsigned long size = 0;
	int times = 0;
	int log_level;
	int out_way;
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

	while(times < 100)
	{
		if (0 == get_video_frame(&data, &size))
		{
			times++;
			printf("video data size = %d\n", size);
			free(data);
		}

		if (0 == get_audio_frame(&data, &size))
		{
			printf("audio data size = %d\n", size);
			free(data);
		}
	}

	stop_capture();

	free_log();

	return 0;
}