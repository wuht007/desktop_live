#include <Windows.h>
#include <stdio.h>
#include <time.h>

#include "log.h"

typedef struct log
{
	int initialized;
	LEVEL log_level;
	OUT_WAY out_way;
	FILE *fp;
	RTL_CRITICAL_SECTION cs;
}LOG;

static LOG s_log = {0};

int init_log(LEVEL level, OUT_WAY out_way)
{
	time_t timer;
	struct tm *tblock;
	char file[MAX_PATH] = {0};

	if (s_log.initialized)
	{
		return INITED;
	}

	s_log.initialized = 1;
	s_log.log_level = level;
	s_log.out_way = out_way;

	timer = time(NULL);
	tblock = localtime(&timer);
	sprintf(file, "./%04d-%02d-%02d-%02d-%02d-%02d.txt",
		tblock->tm_year+1900, tblock->tm_mon+1, tblock->tm_mday,
		tblock->tm_hour, tblock->tm_min, tblock->tm_sec);
	s_log.fp = fopen(file, "w+");
	if (!s_log.fp)
	{
		return OPEN_FAILED;
	}

	InitializeCriticalSection(&s_log.cs);
	return INIT_SECCESS;
}

int print_log(LEVEL level, char *format, ...)
{
	va_list args;
	int i = 0, ret = 0;
	char str[2048] = {0};

	if (0 == s_log.initialized)
	{
		return NOINIT;
	}

	if (level < s_log.log_level)
	{
		return LOW_LEVEL;
	}

	va_start(args, format);
	i = vsprintf(str, format, args);

	EnterCriticalSection(&s_log.cs);

	if (s_log.out_way == OUT_FILE)
	{
		fprintf(s_log.fp, "%d : ", level);
		ret = fwrite(str, 1, i, s_log.fp);
	}
	else if (s_log.out_way == OUT_STDOUT)
	{
		printf("%d %s", level, str);
	}

	LeaveCriticalSection(&s_log.cs);

	return ret;
}

void free_log()
{
	if (0 == s_log.initialized)
	{
		return ;
	}
	s_log.initialized = 0;
	fclose(s_log.fp);
	DeleteCriticalSection(&s_log.cs);
}
