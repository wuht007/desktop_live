#ifndef __DESKTOP_LIVE_H__
#define __DESKTOP_LIVE_H__

typedef struct 
{
	char config_file[MAX_PATH];
	char log_file[MAX_PATH];
	int log_level;
	int log_out_way;

	char record_file[MAX_PATH];
	int record;

	int send_video;
	int send_audio;
	//
	long long i;

	SOCKADDR_IN source;
#define IP_LEN 20
	char server_ip[IP_LEN];
	unsigned short listen_port;
	SOCKET listen_socket;
	FD_SET rfds;
	struct timeval tv;
}SERVER;

#ifdef __cplusplus
extern "C"
{
#endif



#ifdef __cplusplus
};
#endif


#endif