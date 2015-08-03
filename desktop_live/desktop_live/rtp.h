#ifndef __RTP_H__
#define __RTP_H__

#include <WinSock2.h>
#include "list.h"



enum stream_type
{
	video=0,
	audio=1,
	subtitle=2
};

typedef struct 
{
	SOCKET rtp_socket;
	SOCKET rtcp_socket;
	enum stream_type type;
	SOCKADDR_IN dest_addr;

	struct list_head list;
}RTP;

int init_rtp_socket(RTP *rtp);

#endif