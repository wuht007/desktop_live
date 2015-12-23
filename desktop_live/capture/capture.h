#ifndef __CAPTURE_H__
#define __CAPTURE_H__

#include "log.h"
#include "list.h"

#define BUILDING_DLL 1

#if BUILDING_DLL
# define DLLIMPORT __declspec (dllexport)
#else
# define DLLIMPORT __declspec (dllimport)
#endif

typedef unsigned char uint8_t;

typedef struct capture_config
{
	int fps;
	int channels;
	int bits_per_sample;
	int	samples_per_sec;
	int	avg_bytes_per_sec;
}CAPTURECONFIG, *PCAPTURECONFIG;

#define INITED 0
#define WRONG_PARAM -1
#define MALLOCFAILED -2
#define NOINIT -3
#define STARTTHREADFAILED -4
#define WAVEINOPENFAILED -5
#define WAVEINPREPAREHEADERFAILED -6
#define WAVEINADDBUFFERFAILED -7
#define WAVEINSTARTFAILED -8
#define WAVEINSTOPFAILED -9
#define WAVEINRESETFAILED -10
#define WAVEINCLOSEFAILED -11
#define DEQUEUEFAILED -12
#define NOSTART -13
#define NOSTOP -14
#define CREATE_EVENT_FAILED -15
#define SECCESS 1

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct node
{
	uint8_t *data;
	unsigned long size;
	struct list_head list;
}NODE;

typedef struct
{
	uint8_t *yuv;
	uint8_t *rgba;
	unsigned long rgba_len;
	unsigned long yuv_len;
	int fps;
}VIDEO, *PVIDEO;

typedef struct  
{
	//屏幕宽高
	int width;
	int height;

	//屏幕位图的宽高深度通道数
	int bitmap_width;
	int bitmap_height;
	int bitmap_depth;
	int bitmap_channel;

	//一帧位图的数据长度 h*w*d*c
	long len;
}SCREEN, *PSCREEN;

typedef struct 
{
	int channels;//2
	int bits_per_sample;//16
	int samples_per_sec;//48000
	int avg_bytes_per_sec;//48000

	unsigned long pcm_len;
	uint8_t *pcm;
}AUDIO, *PAUDIO;

typedef struct capture
{
	int initialized;
	int started;
#define ARRAY_LEN	2
#define VIDEO_INDEX 0
#define AUDIO_INDEX 1
	HANDLE handler[2];
	HANDLE hEvent[2];
	int stop;
	RTL_CRITICAL_SECTION cs[2];
	struct list_head head[2];
	int width;
	int height;

	AUDIO audio;
	VIDEO video;
	SCREEN screen;
}CAPTURE, *PCAPTURE;

DLLIMPORT PCAPTURE InitCapture(PCAPTURECONFIG pCaptureConfig);

DLLIMPORT int StartCapture(PCAPTURE pCapture);

DLLIMPORT int GetVideoFrame(PCAPTURE pCapture, void **data, unsigned long *size, int *width, int *hetgit);

DLLIMPORT int GetAudioFrame(PCAPTURE pCapture, void **data, unsigned long *size);

DLLIMPORT int StopCapture(PCAPTURE pCapture);

DLLIMPORT int FreeCapture(PCAPTURE pCapture);

#ifdef __cplusplus
};
#endif

#endif //__CAPTURE_H__