#ifndef __ENCODE_H__
#define __ENCODE_H__

#ifdef __cplusplus
extern "C"
{
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"
#ifdef __cplusplus
};
#endif

#define BUILDING_DLL 1

#if BUILDING_DLL
# define DLLIMPORT __declspec (dllexport)
#else
# define DLLIMPORT __declspec (dllimport)
#endif

#define NOT_GET_PACKET 0
#define WRONG_PARAM -1
#define MALLOCFAILED -2
#define NOT_FIND_ENCODER -3
#define NEW_STREAM_FAILED -4
#define SET_OPT_FAILED -5
#define OPEN_CODEC_FAILED -6
#define OPEN_OUTPUT_FILE_FAILED -7
#define WRITE_HEADER_FAILED -8
#define ALLOC_FRAME_FAILED -9
#define GET_FRAME_BUFFER_FAILED -10
#define WRITE_FRAME_FAILED -11
#define SWS_GET_CONTEXT_FAILED -12
#define SWS_SCALE_FAILED -13
#define ENCODE_VIDEO_FRAME_FAILED -14
#define ENCODE_AUDIO_FRAME_FAILED -15
#define AF_ALLOC_OUTPUT -16
#define SECCESS 1


#ifdef __cplusplus
extern "C"
{
#endif

typedef struct AudioPacket
{
	void *data;
	int size;
	long long pts;
	long long dts;
}AUDIOPACKET, *PAUDIOPACKET;

typedef struct encode_config
{
	int fps;
	int	width;
	int	height;
	int	bit_rate;
	int	channels;
	int	bits_per_sample;
	int	sample_rate;
	int	avg_bytes_per_sec;
	char record_file[MAX_PATH];
	int	record;
}ENCODECONFIG, *PENCODECONFIG;

typedef struct encoder
{
	char record_file[MAX_PATH];
	int record;

#define VIDEO_STREAM_INDEX 0
#define AUDIO_STREAM_INDEX 1

	AVFormatContext *fmt_ctx;
	AVCodecContext *video_codec_ctx;
	AVStream *video_stream;
	AVCodec *video_codec;
	AVPacket video_packet;
	AVFrame *video_frame;
	int width;
	int height;
	int fps;
	int bit_rate;
	int video_pts;

	AVCodecContext *audio_codec_ctx;
	AVStream *audio_stream;
	AVCodec *audio_codec;
	AVPacket audio_packet;
	AVFrame *audio_frame;
	int channels;
	int bits_per_sample;
	int sample_rate;
	int avg_bytes_per_sec;
	int audio_pts;
}ENCODER, *PENCODER;

DLLIMPORT PENCODER InitEncoder(PENCODECONFIG pEncodeConfig);
DLLIMPORT int EncodeVideo(PENCODER pEncoder, void *source, int source_width, int source_height,
								void **dest, unsigned long *dest_size, long long *pts, long long *dts);
DLLIMPORT int EncodeAudio(PENCODER pEncoder, void *source, unsigned long source_size, 
								PAUDIOPACKET pAudioPacket, int *ap_len);
DLLIMPORT int FflushEncoder(PENCODER pEncoder);
DLLIMPORT int FreeEncoder(PENCODER pEncoder);

#ifdef __cplusplus
};
#endif

#endif //__ENCODE_H__