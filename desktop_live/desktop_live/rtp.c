#include "rtp.h"

int init_rtp_socket(RTP *rtp)
{
	int ret = 0;
	rtp->rtp_socket = socket(AF_INET, SOCK_DGRAM, 0);

	ret = 0;
	return ret;
}