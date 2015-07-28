#include "rtsp.h"


#ifndef MIN
#  define MIN(a,b)  ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#  define MAX(a,b)  ((a) < (b) ? (b) : (a))
#endif

int parse_recv_buffer(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = -1;
	char *line = NULL;
	char *buffer = recv_buf;
	line = strtok(buffer, "\r\n");
	ret = sscanf(buffer,"%[^ ] %[^ ] ",rtsp->cmd, rtsp->url);
	if (ret != 2)
	{
		ret = -1;
		return ret;
	}

	while (line = strtok(NULL,"\r\n"))
	{
		if (strncmp(line, "CSeq", 4) == 0)
		{
			int len = strlen(line);
			len = MIN(len,10);
			memset(rtsp->cseq, 0, 10);
			strncpy(rtsp->cseq, line, len);
		}
		else if (strncmp(line, "User-Agent", 10) == 0)
		{

		}
		//Accept: application/sdp
		else if (strncmp(line, "Accept: application/sdp", 23) == 0)
		{
			rtsp->mode = 1;
		}
		else if (strncmp(line, "Transport", 9) == 0)
		{
			//URL: rtsp://192.168.109.151/EP01.mkv/track1
			//应该在这里进行解析line Transport: RTP/AVP/UDP;unicast;client_port=32066-32067
			//为了简化程序，我并没有解析
			//先判断是音频流还是视频流，设定track1=video track2=audio
/*			if (get_sub_string_pos_in_string(rtsp->url, "track1") != -1)
			{
				//0是video
				rtp_session *rtp_sess = &rtsp_sess->rtp_connection[0];
				rtp_sess->transport_mode = TRANSPORT_RTP_AVP_UDP_UNICAST;
				rtp_sess->stream_type = STREAM_TYPE_VIDEO;
				get_rtp_client_port(line, &rtp_sess->dest_port);

				init_rtp_socket(rtsp_sess, 0);
				//初始化目的地址
				memcpy((void *)&rtp_sess->dest_addr, (void *)&rtsp_sess->dest, sizeof(SOCKADDR_IN));
				rtp_sess->dest_addr.sin_port = htons(rtp_sess->dest_port);

			}
			else if (get_sub_string_pos_in_string(rtsp->url, "track2") == 0)
			{

				//1是audio
				rtp_session *rtp_sess = &rtsp_sess->rtp_connection[1];
				rtp_sess->transport_mode = TRANSPORT_RTP_AVP_UDP_UNICAST;
				rtp_sess->stream_type = STREAM_TYPE_AUDIO;
				get_rtp_client_port(line, &rtp_sess->dest_port);

				init_rtp_socket(rtsp_sess, 1);
				//初始化目的地址
				memcpy((void *)&rtp_sess->dest_addr, (void *)&rtsp_sess->dest, sizeof(SOCKADDR_IN));
				rtp_sess->dest_addr.sin_port = htons(rtp_sess->dest_port);

			}
*/
		}
	}

	return 0;
}