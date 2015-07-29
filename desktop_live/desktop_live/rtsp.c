#include <string.h>
#include <stdio.h>
#include <time.h>
#include "rtsp.h"



#ifndef MIN
#  define MIN(a,b)  ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#  define MAX(a,b)  ((a) < (b) ? (b) : (a))
#endif

char *get_time(char *time_buf)
{
	time_t t = time(NULL);
	strftime(time_buf,127,"%a,%b %d %y %H:%M:%S",gmtime(&t));
	return time_buf;
}

int handle_options(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = -1;
	rtsp->send_len = sprintf(rtsp->send_buf, "%s 200 OK\r\n"
							"%s\r\n"
							"%s\r\n"
							//"Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, LAPY, PAUSE, GET_PARAMETER, SET_PARAMETER",
							"Public: OPTIONS, DESCRIBE, SETUP, PAPY\r\n\r\n"
							, rtsp->version, rtsp->cseq, rtsp->time_buf);

	ret = 0;
	return ret;
}

int handle_describe(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = -1;
	char describe[20] = {0};
	char *application = strstr(recv_buf, "application");
	if (NULL == application)
	{
		ret = -1;
		return ret;
	}

	ret = sscanf(application, "%[^\r\n]\r\n",)

	rtsp->send_len = sprintf(rtsp->send_buf, "%s 200 OK\r\n"
		"%s\r\n"
		"%s\r\n"
		"Content-Base: %s"
		"Content-Type: %s"
		, rtsp->version, rtsp->cseq, rtsp->time_buf, rtsp->url);

	ret = 0;
	return ret;
}

int handle_setup(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = -1;
	return ret;
}

int handle_play(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = -1;
	return ret;
}

int handle_void()
{
	int ret = -1;
	return ret;
}

int parse_recv_buffer(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = -1;
	char *buffer = recv_buf;
	char *cseq = NULL;

	memset(rtsp->cmd, 0, sizeof(rtsp->cmd));
	memset(rtsp->url, 0, sizeof(rtsp->url));
	memset(rtsp->version, 0, sizeof(rtsp->version));
	memset(rtsp->cseq, 0, sizeof(rtsp->cseq));
	memset(rtsp->time_buf, 0, sizeof(rtsp->time_buf));

	ret = sscanf(buffer,"%[^ ] %[^ ] %[^ \r\n]\r\n",rtsp->cmd, rtsp->url, rtsp->version);
	if (ret != 3)
	{
		ret = -1;
		return ret;
	}

	cseq = strstr(buffer, "CSeq");
	if (NULL == cseq)
	{
		ret = -2;
		return -2;
	}

	ret = sscanf(cseq, "%[^\r\n]\r\n", rtsp->cseq);
	if (ret != 1)
	{
		ret = -3;
		return ret;
	}

	get_time(rtsp->time_buf);

	if(0 == strncmp(buffer, "OPTIONS", strlen("OPTIONS")))
		handle_options(rtsp, buffer, size);
	else if(0 == strncmp(buffer, "DESCRIBE", strlen("DESCRIBE")))
		handle_describe(rtsp, buffer, size);
	else if(0 == strncmp(buffer, "SETUP", strlen("SETUP")))
		handle_setup(rtsp, buffer, size);
	else if(0 == strncmp(buffer, "PLAY", strlen("PLAY")))
		handle_play(rtsp, buffer, size);
	else
		handle_void();

	send_rtsp(rtsp);

	return 0;
}

int send_rtsp(RTSP *rtsp)
{
	return send(rtsp->rtsp_socket, rtsp->send_buf, rtsp->send_len, 0);
}