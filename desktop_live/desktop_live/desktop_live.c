#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string.h>
#include "desktop_live.h"
#include "capture.h"
#include "log.h"
#include "rtsp.h"
#include "encode.h"
#include "rtp.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "log.lib")
#pragma comment(lib, "capture.lib")
#pragma comment(lib, "encode.lib")

//返回值 0=成功 其他值=失败
//填充SERVER->config_file
int get_config_file(SERVER *server)
{
	int ret = 0;
	char *last = NULL;
	char *config_file = "config.ini";
	int size = strlen(config_file);

	ret = GetModuleFileName(NULL, server->config_file, MAX_PATH);
	if (ret <= 0)
	{
		ret = -1;
		return ret;
	}

	last = strrchr(server->config_file, '\\');
	if (NULL == last)
	{
		ret = -2;
		return ret;
	}
	//"E:\desktop_live\desktop_live\Debug\..\lib\desktop_live.exe"
	//											|
	//											last
	//											 |
	//											 last+1
	last = last + 1;

	memcpy(last, config_file, size);
	*(last+size) = '\0';

	ret = 0;
	return ret;
}

//0=成功 其他=失败
//执行WSAStartup
int init_windows_socket()
{
	int ret = 0;
	WORD V;
	WSADATA s;

	V = MAKEWORD(1,1);
	ret = WSAStartup(V,&s);
	if(ret != 0)
	{
		return ret;
	}

	ret = 0;
	return ret;
}

//0=成功 其他=失败
//建立SERVER->listen_socket
int set_up_listen_socket(SERVER *server)
{
	int ret = 0;
	unsigned short port = server->listen_port;
	char *ip = server->server_ip;
	
	ret = init_windows_socket();
	if (ret != 0)
	{
		ret = -1;
		return ret;
	}

	server->listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server->listen_socket < 0)
	{
		ret = -2;
		return ret;
	}

	server->source.sin_family = AF_INET;
	server->source.sin_port = htons(port);
	inet_pton(AF_INET, ip, &server->source.sin_addr);

	ret = bind(server->listen_socket, (SOCKADDR*)&server->source, sizeof(SOCKADDR));
	if (ret < 0)
	{
		ret = -3;
		return ret;
	}

	ret = listen(server->listen_socket, 5);
	if (ret < 0)
	{
		ret = -4;
		return ret;
	}

	ret = 0;
	return ret;
}

//0=成功 其他=失败
//从config.ini中读取日志文件、级别、输出方式，流媒体录制文件、录制标志
//server的ip和端口号,select超时时间
int init_basic_param(SERVER *server)
{
	int ret = 0;
	char *config_file = server->config_file;

	GetPrivateProfileString("log", "log_file", "D:\\desktop_live_log_default.txt", 
									server->log_file, MAX_PATH, config_file);
	server->log_level = GetPrivateProfileIntA("log", "level", 0, config_file);
	server->log_out_way = GetPrivateProfileIntA("log", "out_way", 0, config_file);


	GetPrivateProfileString("encode", "record_file", "D:\\desktop_live.mp4", 
								server->record_file, MAX_PATH, config_file);
	server->record = GetPrivateProfileIntA("encode", "record", 1, config_file);

	GetPrivateProfileString("live", "server_ip", "192.168.109.151", 
								server->server_ip, IP_LEN, config_file);
	server->listen_port = GetPrivateProfileIntA("live", "listen_port", 554, config_file);

	server->tv.tv_sec = GetPrivateProfileIntA("live", "tv_sec", 0, config_file);
	server->tv.tv_usec = GetPrivateProfileIntA("live", "tv_usec", 50000, config_file);

	ret = 0;
	return ret;
}

char *find_nalu(char **data, int size, int *len)
{
	char *nalu = NULL;
	char *nalu_end = NULL;
	int finded = 0;
	nalu = *data;
	while(size-- >= 0)
	{
		if ((*nalu == 0x00 && *(nalu+1) == 0x00 && *(nalu+2) == 0x01)
			|| (*nalu == 0x00 && *(nalu+1) == 0x00 && *(nalu+2) == 0x00 && *(nalu+3) == 0x01))
		{
			*len = 3;
			nalu_end = nalu + 3;
			while(*len <= size)
			{
				if ((*nalu_end == 0x00 && *(nalu_end+1) == 0x00 && *(nalu_end+2) == 0x01)
					|| (*nalu_end == 0x00 && *(nalu_end+1) == 0x00 && *(nalu_end+2) == 0x00 && *(nalu_end+3) == 0x01))
				{
					break;
				}

				nalu_end++;
				(*len)++;
			}
			*data = nalu_end;
			finded = 1;
			break;
		}

		nalu++;
	}

	if (finded)
	{
		return nalu;
	}

	return NULL;
}

int send_rtp(struct list_head *rtsp_head, char *send_buf, int size, int type)
{
	int ret = 0;
	struct list_head *p_rtsp_list = NULL;
	struct list_head *p_rtp_list = NULL;
	list_for_each(p_rtsp_list, rtsp_head)
	{
		RTSP *rtsp = list_entry(p_rtsp_list, RTSP, list);
		list_for_each(p_rtp_list, &rtsp->rtp_head) 
		{
			RTP *rtp = list_entry(p_rtp_list, RTP, list);
			if (rtp->type == type)//stream_type::video)
			{
				ret = sendto(rtp->rtp_socket, send_buf, size, 0, 
						(SOCKADDR *)&rtp->dest_addr, sizeof(SOCKADDR));
				printf("send len = %d\n", ret);
			}
		}
	}
	
	ret = 0;
	return ret;
}

int send_media(SERVER *server, struct list_head *rtsp_head)
{
	int ret = 0;
	char *data = NULL;
	unsigned long size = 0;
	int width = 0;
	int height = 0;
	char *dest = NULL;
	unsigned long dest_size = 0;
	long long pts = 0;
	long long dts = 0;

	if (0 == get_video_frame(&data, &size, &width, &height))
	{
		if (0 == encode_video(data, width, height, &dest, &dest_size, &pts, &dts))
		{
			char *encode_data = dest;
			int nalu_len = 0;
			char *nalu = NULL;
			while((nalu = find_nalu(&encode_data, dest_size, &nalu_len)) != NULL)
			{
				static unsigned short sq = 34763;
				char *nal = nalu;
				int nal_len = nalu_len;
				//RTP_HEADER rtp_hdr = {0};
				char rtp_hdr[12] = {0};
				char imp = 0;
				char type = 0;
				char send_buf[1500] = {0};

				dest_size -= nalu_len;

				while(*(nal++) != 0x01)
					nal_len--;
				nal_len--;

				imp &= 0x0;
				imp |= (*nal);
				imp &= 0xe0;

				type &= 0x0;
				type |= (*nal);
				type &= 0x1f;

/*				rtp_hdr.version = 2;
				rtp_hdr.padding = 0;
				rtp_hdr.extension = 0;
				rtp_hdr.csrc_len = 0;
//				rtp_hdr.marker = 0;
				rtp_hdr.payloadtype = 96;
//				rtp_hdr.seq_no = htons(sq++);
				rtp_hdr.timestamp = htonl(pts/1024*9000);
				rtp_hdr.ssrc = htonl(2906685981);
*/
				rtp_hdr[0] = 0x80;
				rtp_hdr[1] = 0xe0;
				*(unsigned short *)&rtp_hdr[4] = htonl(pts/1024*9000);
				*(unsigned short *)&rtp_hdr[8] = htonl(2906685981);

				if (nal_len < 1400)
				{
//					rtp_hdr.seq_no = htons(sq++);
//					rtp_hdr.marker = 1;
					*(unsigned short *)&rtp_hdr[2] = htons(sq++);
					memcpy(send_buf, &rtp_hdr, 12);
					memcpy(send_buf+12, nal, nal_len);

					send_rtp(rtsp_head, send_buf, 12+nal_len, 0);//stream_type::video);
					nal_len -= nal_len;
					nal += nal_len;
					continue;
				}

				while(nal_len > 0)
				{
					int first = 0;
//					rtp_hdr.seq_no = htons(sq++);
					*(unsigned short *)&rtp_hdr[2] = htons(sq++);
					if (nal_len < 1400)
					{
//						rtp_hdr.marker = 1;
						rtp_hdr[1] = 0xe0;
						memcpy(send_buf, &rtp_hdr, 12);
						send_buf[12] |= 0x1c;
						send_buf[12] |= imp;

						send_buf[13] |= 0x40;
						send_buf[13] |= type;
						memcpy(send_buf+14, nal, nal_len);

						send_rtp(rtsp_head, send_buf, 14+nal_len, 0);//stream_type::video);
						nal_len -= nal_len;
						nal += nal_len;
					}
					else
					{
//						rtp_hdr.marker = 0;
						rtp_hdr[1] = 0x60;
						memcpy(send_buf, &rtp_hdr, 12);
						//0x1c=28 FU-A
						send_buf[12] = 0x1c;
						send_buf[12] |= imp;

						if (first == 0)
						{
							first = 1;
							send_buf[13] |= 0x80;
							send_buf[13] |= type;
							//去掉前面的开始码 如 65 67 68等等
							nal += 1;
							nal_len -= 1;
						}
						else
						{
							send_buf[13] &= 0x0;
							send_buf[13] |= type;
						}

						memcpy(send_buf+14, nal, 1400);

						send_rtp(rtsp_head, send_buf, 14+1400, 0);//stream_type::video);
						nal_len -= 1400;
						nal += 1400;
					}
				}
			}
			free(dest);
		}
		free(data);
	}

	if (0 == get_audio_frame(&data, &size))
	{
//		printf("audio data size = %d\n", size);
		free(data);
	}

	ret = 0;
	return ret;
}

int add_client(SERVER *server, struct list_head *rtsp_head)
{
	int ret = 0;
	int size = sizeof(SOCKADDR_IN);
	RTSP *rtsp = NULL;

	rtsp = (RTSP *)malloc(sizeof(RTSP));
	if (NULL == rtsp)
	{
		ret = -1;
		return ret;
	}

	memset(rtsp, 0, sizeof(RTSP));
	INIT_LIST_HEAD(&rtsp->rtp_head);
	rtsp->server_ip = server->server_ip;

	rtsp->rtsp_socket = accept(server->listen_socket, (SOCKADDR *)&rtsp->dest_addr, &size);
	list_add(&rtsp->list, rtsp_head);

	ret = 0;
	return ret;
}

int handle_recv(RTSP *rtsp, char *recv_buf, int size)
{
	int ret = 0;
	ret = parse_recv_buffer(rtsp, recv_buf, size);
	if (0 != ret)
	{
		ret = -1;
		return ret;
	}

	ret = 0;
	return ret;
}

int do_read(SERVER *server, struct list_head *rtsp_head)
{
	int ret = 0;
	struct list_head *plist;

	if (FD_ISSET(server->listen_socket,&server->rfds))
	{
		ret = add_client(server, rtsp_head);
	}

	list_for_each(plist, rtsp_head)
	{
		RTSP *rtsp = list_entry(plist, RTSP, list);
		if (FD_ISSET(rtsp->rtsp_socket, &server->rfds))
		{
			char recv_buf[2048] = {0};
			char send_buf[2048] = {0};
			ret = recv(rtsp->rtsp_socket, recv_buf, 2048, 0);
			if (ret > 0)
			{
				rtsp->send_buf = send_buf;
				handle_recv(rtsp, recv_buf, ret);
			}
			else
			{
				//连接失效，释放所有该rtsp会话的资源
			}
		}
	}

	ret = 0;
	return ret;
}

int main(int argc, char **argv)
{
	int ret = 0;
	SERVER server = {0};
	LOG *log = NULL;
	struct list_head rtsp_head = {0};
	long long i = 1000;
	ret = get_config_file(&server);
	if (ret != 0)
	{
		ret = -1;
		return ret;
	}

	init_basic_param(&server);

	log = init_log(server.log_file, server.log_level, server.log_out_way);
	if (log == NULL)
	{
		ret = -2;
		return ret;
	}

	INIT_LIST_HEAD(&rtsp_head);

	ret = set_up_listen_socket(&server);
	if (ret != 0)
	{
		ret = -3;
		return ret;
	}

	FD_ZERO(&server.rfds);
	FD_SET(server.listen_socket, &server.rfds);

	ret = start_capture(log, server.config_file);
	if (ret != 0)
	{
		ret = -4;
		return ret;
	}

	ret = init_ercoder(log, server.config_file, server.record_file, server.record);
	if (ret != 0)
	{
		ret = -5;
		return ret;
	}

	while (i--)
	{
		struct list_head *plist;
		ret = select(0, &server.rfds, NULL, NULL, &server.tv);
		if (ret < 0)
		{
			ret = -5;
			break;
		}
		else if (ret == 0)
		{
			ret = send_media(&server, &rtsp_head);
		}
		else
		{
			do_read(&server, &rtsp_head);
		}

		FD_SET(server.listen_socket, &server.rfds);
		list_for_each(plist, &rtsp_head)
		{
			RTSP *rtsp = list_entry(plist, RTSP, list);
			FD_SET(rtsp->rtsp_socket, &server.rfds);
		}
	}

	stop_capture();

	fflush_encoder();

	free_encoder();

	free_log();

	return ret;
}