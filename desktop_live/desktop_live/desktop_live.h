#ifndef __DESKTOP_LIVE_H__
#define __DESKTOP_LIVE_H__

#include "list.h"
#include <WinSock2.h>
#include "log.h"

enum stream_type
{
	video=0,
	audio=1,
	subtitle=2
};
enum transport_mode
{
	udp=0,
	other
};

typedef struct 
{
	SOCKET rtp_socket;
	SOCKET rtcp_socket;
	enum stream_type type;
	SOCKADDR_IN dest_addr;
	enum transport_mode mode;
}RTP;

typedef struct 
{
	SOCKET rtsp_socket;
	char *cmd;
	char *url;
	char *cseq;
	char *time_buf;
	void *media;
	SOCKADDR_IN dest;

	struct list_head head;
}RTSP;

typedef struct 
{
	char *config_file;
	char *log_file;
	LOG *log;
	SOCKADDR_IN source;
	SOCKET listen_socket;
	RTSP *rtsp_list;
}SERVER;

#ifdef __cplusplus
extern "C"
{
#endif



#ifdef __cplusplus
};
#endif


#endif