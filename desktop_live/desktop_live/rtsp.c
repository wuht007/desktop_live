#include <string.h>
#include <stdio.h>
#include <time.h>
#include <WS2tcpip.h>
#include "rtsp.h"
#include "rtp.h"

const unsigned short rtp_video_port = 1676;
const unsigned short rtp_audio_port = 1876;

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

int get_sdp_line(RTSP* rtsp, char *sdp, int *len)
{
	//根据rtsp->media_info，但是我并没有做哪些
	//此处默认视频流为track1，音频流为track2
	*len = sprintf(sdp,"v=0\r\n"
		"o=- 123456789 1 IN IP4 192.168.109.151\r\n"
		"s=desktop live\r\n"
		"i=desktop_live\r\n"
		"t=0 0\r\n"
		"a=tool:wht_rtsp_server\r\n"
		"a=type:broadcast\r\n"
		"a=control:*\r\n"
		"a=range:npt=0-\r\n"
		"a=x-qt-text-nam:H.264 Video, streamed by the LIVE555 Media Server\r\n"
		"a=x-qt-text-inf:desktop_live\r\n"
		"m=video 0 RTP/AVP 96\r\n"
		"c=IN IP4 0.0.0.0\r\n"
		"b=AS:400\r\n"
		"a=rtpmap:96 H264/90000\r\n"
		"a=fmtp:96 packetization-mode=1;profile-level-id=640020;sprop-parameter-sets=Z2QAIKzZQFYGHm4QAAADABAAAAMDKPGDGWA=,aOvjyyLA\r\n"
		"a=control:track1\r\n"
		//"m=audio 0 RTP/AVP 97\r\n"
		//					   "c=IN IP4 0.0.0.0\r\n"
		//					   "b=AS:96\r\n"
		//					   "a=rtpmap:97 MPEG4-GENERIC/48000/2\r\n"
		//"a=control:track2\r\n"
		);
	return *len;
}

int handle_options(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = -1;
	rtsp->send_len = sprintf(rtsp->send_buf, "%s 200 OK\r\n"
							"%s\r\n"
							"Date: %s\r\n"
							//"Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, LAPY, PAUSE, GET_PARAMETER, SET_PARAMETER",
							"Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n\r\n"
							, rtsp->version, rtsp->cseq, rtsp->time_buf);

	ret = 0;
	return ret;
}

int handle_describe(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = -1;
	char describe[20] = {0};
	char sdp[1024] = {0};
	int sdp_len = 0;
	char *application = strstr(recv_buf, "application");
	
	if (NULL == application)
	{
		ret = -1;
		return ret;
	}

	ret = sscanf(application, "%[^\r\n]\r\n", describe);

	get_sdp_line(rtsp, sdp, &sdp_len);

	rtsp->send_len = sprintf(rtsp->send_buf, "%s 200 OK\r\n"
		"%s\r\n"
		"Date: %s\r\n"
		"Content-Base: %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n\r\n"
		"%s"
		, rtsp->version, rtsp->cseq, rtsp->time_buf, rtsp->url, describe, sdp_len, sdp);

	ret = 0;
	return ret;
}

int handle_setup(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = -1;
	unsigned short client_rtp_port = 0;
	unsigned short server_rtp_port = 0;
	char destination[21] = {0};
	RTP *rtp = NULL;
	char *client_port = strstr(recv_buf, "client_port=");

	if (NULL == client_port)
	{
		ret = -1;
		return ret;
	}

	client_port += strlen("client_port=");
	client_rtp_port = atoi(client_port);

	rtp = (RTP *)malloc(sizeof(RTP));
	if (NULL == rtp)
	{
		ret = -2;
		return ret;
	}

	memset(rtp, 0 ,sizeof(RTP));
	memcpy(&rtp->dest_addr, &rtsp->dest_addr, sizeof(SOCKADDR_IN));
	rtp->dest_addr.sin_port = htons(client_rtp_port);

	init_rtp_socket(rtp);

	if (strstr(recv_buf, "track1"))
	{
		server_rtp_port = rtp_video_port;
		rtp->type = 0;//stream_type::video;
	}
	else if (strstr(recv_buf, "track2"))
	{
		server_rtp_port = rtp_audio_port;
		rtp->type = 1;//stream_type::audio;
	}

	list_add(&rtp->list, &rtsp->rtp_head);
	inet_ntop(AF_INET, &rtsp->dest_addr.sin_addr, destination, 21);

	rtsp->send_len = sprintf(rtsp->send_buf, "%s 200 OK\r\n"
		"%s\r\n"
		"Date: %s\r\n"
		"Transport: RTP/AVP;unicast;destination=%s;"
		"source=%s;"
		"client_port=%d-%d;"
		"server_port=%d-%d\r\n"
		"Session: 0465AEC9;timeout: 65\r\n\r\n"
		, rtsp->version, rtsp->cseq, rtsp->time_buf, destination, rtsp->server_ip, 
		client_rtp_port, client_rtp_port+1, 
		server_rtp_port, server_rtp_port+1);

	ret = 0;
	return ret;
}

int handle_play(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = -1;
	rtsp->send_len = sprintf(rtsp->send_buf, 
		"%s 200 OK\r\n"
		"%s\r\n"
		"Date: %s\r\n\r\n"
		, rtsp->version, rtsp->cseq, get_time(rtsp->time_buf));



	ret = 0;
	return ret;
}

int handle_void()
{
	int ret = -1;
	return ret;
}

int send_rtsp(RTSP *rtsp)
{
	return send(rtsp->rtsp_socket, rtsp->send_buf, rtsp->send_len, 0);
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

	ret = send_rtsp(rtsp);

	return 0;
}