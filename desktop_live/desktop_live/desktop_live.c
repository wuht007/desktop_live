#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string.h>
#include "desktop_live.h"
#include "capture.h"
#include "log.h"
#include "rtsp.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "log.lib")
#pragma comment(lib, "capture.lib")

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

int send_media(SERVER *server)
{
	int ret = 0;
	char *data = NULL;
	unsigned long size = 0;
	int width = 0;
	int height = 0;

	if (0 == get_video_frame(&data, &size, &width, &height))
	{
//		printf("video data size = %d\n", size);
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

	rtsp->rtsp_socket = accept(server->listen_socket, (SOCKADDR *)&rtsp->client, &size);
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

	while (1)
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
			ret = send_media(&server);
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

	return ret;
}