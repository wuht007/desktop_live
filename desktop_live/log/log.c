#include <Windows.h>
#include <stdio.h>
#include <time.h>

#include "log.h"

typedef struct log
{
	int initialized;
	LEVEL logLevel;
	OUTWAY outWay;
	FILE *fp;
	RTL_CRITICAL_SECTION cs;
}LOG;

static LOG s_log = {0,LOG_DEBUG,OUT_STDOUT,NULL,0};

int InitLog(LEVEL level, OUTWAY outWay)
{
	time_t timer;
	struct tm *tblock;
	char file[MAX_PATH] = {0};

	if (s_log.initialized)
	{
		return INITED;
	}

	s_log.logLevel = level;
	s_log.outWay = outWay;

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

	s_log.initialized = 1;
	return INIT_SECCESS;
}

int PrintLog(LEVEL level, char *format, ...)
{
	va_list args;
	int i = 0, ret = 0;
	char str[2048] = {0};

	if (0 == s_log.initialized)
	{
		return NOINIT;
	}

	if (level < s_log.logLevel)
	{
		return LOW_LEVEL;
	}

	va_start(args, format);
	i = vsprintf(str, format, args);

	EnterCriticalSection(&s_log.cs);

	if (s_log.outWay == OUT_FILE)
	{
		fprintf(s_log.fp, "%d : %d : ", GetCurrentThreadId(), level);
		ret = fwrite(str, 1, i, s_log.fp);
		fflush(s_log.fp);
	}
	else if (s_log.outWay == OUT_STDOUT)
	{
		printf("%d %s", level, str);
	}

	LeaveCriticalSection(&s_log.cs);

	return ret;
}

void FreeLog()
{
	if (0 == s_log.initialized)
	{
		return ;
	}
	s_log.initialized = 0;
	fclose(s_log.fp);
	DeleteCriticalSection(&s_log.cs);
}
