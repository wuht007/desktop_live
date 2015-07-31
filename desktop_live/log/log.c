#include "log.h"

LOG *g_log = NULL;

//返回值:>=0 表示成功 <0 表示失败
//描述:根据log中的系数输出日志
int print_log(LOG *log, unsigned int log_level, char *log_str)
{
	int ret = 0;
	if (NULL == log)
	{
		ret = -1;
		return ret;
	}
	if (log_level < (unsigned int)log->log_priv.log_level)
	{
		ret = -2;
		return ret;
	}

	if (log->log_priv.out_way == OUT_FILE)
	{
		EnterCriticalSection(&log->log_priv.cs);
		fprintf(log->log_priv.file,"%d ",log_level);
		ret = fwrite(log_str, strlen((const char *)log_str), 1, log->log_priv.file);
		fflush(log->log_priv.file);
		LeaveCriticalSection(&log->log_priv.cs);
		return ret;
	} 
	else if (log->log_priv.out_way == OUT_STDOUT)
	{
		EnterCriticalSection(&log->log_priv.cs);
		printf("%s\n", log_str);
		fflush(stdout);
		LeaveCriticalSection(&log->log_priv.cs);
	}

	return 0;
}

LOG *init_log(char *file_name, unsigned int log_level, unsigned int out_way)
{
	if (NULL != g_log)
	{
		//不推荐使用init_log获取log,请使用传参的方式
		return NULL;
	}

	g_log = (LOG *)malloc(sizeof(LOG));
	if (NULL == g_log)
	{
		return NULL;
	}
	memset(g_log, 0, sizeof(LOG));

	g_log->log_priv.log_level = (enum LEVEL)log_level;
	g_log->log_priv.out_way = (enum OUT_WAY)out_way;
	g_log->log_priv.file = fopen(file_name, "w+");
	if (NULL == g_log->log_priv.file)
	{
		free(g_log);
		return NULL;
	}
	InitializeCriticalSection(&g_log->log_priv.cs);

	return g_log;
}

void free_log()
{
	if (NULL == g_log)
	{
		return ;
	}
	fclose(g_log->log_priv.file);
	DeleteCriticalSection(&g_log->log_priv.cs);
	free(g_log);
}

