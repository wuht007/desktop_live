#include "capture.h"
#include "list.h"
#include "log.h"
#include "encode.h"
#include <Windows.h>
#include <process.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#ifdef __cplusplus
};
#endif

#pragma comment(lib, "log.lib")

typedef struct 
{
	AVFormatContext *fmt_ctx;

	AVCodecContext *video_codec_ctx;
	AVStream *video_stream;
	AVCodec *video_codec;
	AVPacket video_packet;
	AVFrame *video_frame;
	int width;
	int height;

	AVCodecContext *audio_codec_ctx;
	AVStream *audio_stream;
	AVCodec *audio_codec;
	AVPacket audio_packet;
	AVFrame *audio_frame;
}ENCODER;

typedef struct global_variable
{
#define VIDEO_INDEX 0
#define AUDIO_INDEX 1
	HANDLE handler;
	int stop;
	void *log_file;
	char config_file[MAX_PATH];
	char record_file[MAX_PATH];
	bool record;
	RTL_CRITICAL_SECTION cs;
	struct list_head head;
}GV;

static GV *gv = NULL;

int init_encoder(GV *global_var, ENCODER *encoder, void *log)
{
	int ret = 0;

	encoder->width = GetPrivateProfileIntA("video", 
		"width", 480, global_var->config_file);
	encoder->height = GetPrivateProfileIntA("video", 
		"height", 320, global_var->config_file);

	av_register_all();

	avformat_alloc_output_context2(&encoder->fmt_ctx, NULL, NULL, global_var->record_file);
	if (!encoder->fmt_ctx)
	{

	}

	ret = 0;
	return ret;
}

unsigned int __stdcall encode_proc(void *p)
{
	int ret = 0;
	GV *global_var = (GV *)p;
	ENCODER encoder;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)global_var->log_file, LOG_DEBUG, log_str);


	ret = init_encoder(global_var, &encoder, global_var->log_file);

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)global_var->log_file, LOG_DEBUG, log_str);

	ret = 0;
	return ret;
}

//返回值 0=成功 其他=失败
//描述:开始编码
int start_encode(void *log_file, char *config_file, char *record_file, bool record)
{
	int ret = 0;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)log_file, LOG_DEBUG, log_str);

	if (gv != NULL)
	{
		ret = -1;
		return ret;
	}

	gv = (GV *)malloc(sizeof(GV));
	if (NULL == gv)
	{
		ret = -2;
		return ret;
	}

	memset(gv, 0, sizeof(GV));
	gv->stop = 0;
	gv->log_file = log_file;
	memcpy(gv->config_file, config_file, strlen(config_file));
	memcpy(gv->record_file, record_file, strlen(record_file));
	gv->record = record;

	INIT_LIST_HEAD(&gv->head);
	InitializeCriticalSection(&gv->cs);
	
	gv->handler = (HANDLE)_beginthreadex(NULL, 0, encode_proc, gv, 0, NULL);
	if (NULL == gv->handler)
	{
		free(gv);
		gv = NULL;
		ret = -4;
		return ret;
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)log_file, LOG_DEBUG, log_str);

	ret = 0;
	return ret;
}

int get_video_packet(void **data, unsigned long *size, long long pts, long long dts)
{
	int ret = 0;
	return ret;
}

int get_audio_packet(void **data, unsigned long *size, long long pts, long long dts)
{
	int ret = 0;
	return ret;
}

int stop_encode()
{
	int ret = 0;
	return ret;
}