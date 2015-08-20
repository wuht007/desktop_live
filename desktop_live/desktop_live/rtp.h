#ifndef __RTP_H__
#define __RTP_H__

#include <WinSock2.h>
#include "list.h"

/*
+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|F|NRI|  Type   |
+---------------+
*/
typedef struct
{
	//byte 0
	unsigned char TYPE:5;
	unsigned char NRI:2;
	unsigned char F:1;
} NALU_HEADER; // 1 BYTE

/*
+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|F|NRI|  Type   |
+---------------+
*/
typedef struct
{
	//byte 0
	unsigned char TYPE:5;
	unsigned char NRI:2;
	unsigned char F:1;
} FU_INDICATOR; // 1 BYTE

/*
+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|S|E|R|  Type   |
+---------------+
*/
typedef struct
{
	//byte 0
	unsigned char TYPE:5;
	unsigned char R:1;
	unsigned char E:1;
	unsigned char S:1;
} FU_HEADER;   // 1 BYTES

typedef struct
{
	unsigned char cc : 4;
	unsigned char x : 1;
	unsigned char p : 1;
	unsigned char v : 2;

	unsigned char pt : 7;
	unsigned char m : 1;

	unsigned short sn;
	unsigned int timestamp;
	unsigned int ssrc;
}RTP_HEADER;

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