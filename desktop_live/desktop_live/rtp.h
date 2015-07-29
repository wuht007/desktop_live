#ifndef __RTP_H__
#define __RTP_H__

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

#endif