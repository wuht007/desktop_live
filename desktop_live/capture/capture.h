#ifndef __CAPTURE_H__
#define __CAPTURE_H__

#include "log.h"

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
#define SECCESS 1

#ifdef __cplusplus
extern "C"
{
#endif

DLLIMPORT int InitCapture(PCAPTURECONFIG pCaptureConfig);

DLLIMPORT int StartCapture();

DLLIMPORT int GetVideoFrame(void **data, unsigned long *size, int *width, int *hetgit);

DLLIMPORT int GetAudioFrame(void **data, unsigned long *size);

DLLIMPORT int StopCapture();

DLLIMPORT int FreeCapture();

#ifdef __cplusplus
};
#endif

#endif //__CAPTURE_H__