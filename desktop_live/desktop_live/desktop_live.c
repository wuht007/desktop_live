#include "desktop_live.h"
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

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

int set_up_listen_socket(SERVER *server)
{
	int ret = 0;
	
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
	//server->source.sin_port = htons(LISTEN_PORT);
	//inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
	ret = 0;
	return ret;
}

int main(int argc, char **argv)
{
	int ret = 0;
	SERVER server = {0};

	ret = set_up_listen_socket(&server);
	return ret;
}