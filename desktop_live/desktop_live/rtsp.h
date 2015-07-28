#ifndef __RTSP_H__
#define __RTSP_H__

#include "list.h"
#include <WinSock2.h>

enum describe_mode
{
	none=0,
	sdp=1
};

typedef struct 
{
	SOCKET rtsp_socket;
	char cmd[20];
	char url[260];
	char cseq[10];
	enum describe_mode mode;
	char time_buf[128];
	void *media;
	SOCKADDR_IN client;

	struct list_head list;
}RTSP;

int parse_recv_buffer(RTSP *rtsp, char *recv_buf, int size);

#endif