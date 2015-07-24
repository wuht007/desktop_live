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
	SOCKADDR_IN client;

	struct list_head list;
}RTSP;

typedef struct 
{
	char config_file[MAX_PATH];

	LOG *log;
	char log_file[MAX_PATH];
	int log_level;
	int log_out_way;

	char record_file[MAX_PATH];
	int record;

	SOCKADDR_IN source;
#define IP_LEN 20
	char server_ip[IP_LEN];
	unsigned short listen_port;
	SOCKET listen_socket;
	FD_SET rfds;
	struct timeval tv;
	struct list_head rtsp_head;
}SERVER;

#ifdef __cplusplus
extern "C"
{
#endif



#ifdef __cplusplus
};
#endif


#endif