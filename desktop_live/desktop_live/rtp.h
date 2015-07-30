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
	/*  0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|V=2|P|X|  CC   |M|     PT      |       sequence number         |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                           timestamp                           |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|           synchronization source (SSRC) identifier            |
	+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	|            contributing source (CSRC) identifiers             |
	|                             ....                              |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
	//intel 的cpu 是intel为小端字节序（低端存到底地址） 而网络流为大端字节序（高端存到低地址）
	/*intel 的cpu ： 高端->csrc_len:4 -> extension:1-> padding:1 -> version:2 ->低端
	 在内存中存储 ：
	 低->4001（内存地址）version:2
	     4002（内存地址）padding:1
		 4003（内存地址）extension:1
	 高->4004（内存地址）csrc_len:4

     网络传输解析 ： 高端->version:2->padding:1->extension:1->csrc_len:4->低端  (为正确的文档描述格式)

	 存入接收内存 ：
	 低->4001（内存地址）version:2
	     4002（内存地址）padding:1
	     4003（内存地址）extension:1
	 高->4004（内存地址）csrc_len:4
	 本地内存解析 ：高端->csrc_len:4 -> extension:1-> padding:1 -> version:2 ->低端 ，
	 即：
	 unsigned char csrc_len:4;        // expect 0 
	 unsigned char extension:1;       // expect 1
	 unsigned char padding:1;         // expect 0 
	 unsigned char version:2;         // expect 2 
	*/
	/* byte 0 */
	 unsigned char csrc_len:4;        /* expect 0 */
	 unsigned char extension:1;       /* expect 1, see RTP_OP below */
	 unsigned char padding:1;         /* expect 0 */
	 unsigned char version:2;         /* expect 2 */
	/* byte 1 */
	 unsigned char payloadtype:7;     /* RTP_PAYLOAD_RTSP */
	 unsigned char marker:1;          /* expect 1 */
	/* bytes 2,3 */
	 unsigned int seq_no;            
	/* bytes 4-7 */
	 unsigned int timestamp;        
	/* bytes 8-11 */
	 unsigned int ssrc;              /* stream number is used here. */
} RTP_HEADER;

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