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

	//video
	AVCodecContext *video_codec_ctx;
	AVStream *video_stream;
	AVCodec *video_codec;
	AVPacket video_packet;
	AVFrame *video_frame;
	int width;
	int height;
	int fps;
	int bit_rate;

	//audio
	AVCodecContext *audio_codec_ctx;
	AVStream *audio_stream;
	AVCodec *audio_codec;
	AVPacket audio_packet;
	AVFrame *audio_frame;
	int samples_per_sec;
	int channels;
	int avg_bytes_per_sec;
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
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)global_var->log_file, LOG_DEBUG, log_str);

	//配置文件中的参数
	encoder->width = GetPrivateProfileIntA("video", 
		"width", 480, global_var->config_file);
	encoder->height = GetPrivateProfileIntA("video", 
		"height", 320, global_var->config_file);
	encoder->fps = GetPrivateProfileIntA("video", 
		"fps", 10, global_var->config_file);
	encoder->bit_rate = GetPrivateProfileIntA("video", 
		"bit_rate", 400000, global_var->config_file);
	encoder->samples_per_sec = GetPrivateProfileIntA("audio", 
		"samples_per_sec", 48000, global_var->config_file);
	encoder->channels = GetPrivateProfileIntA("audio", 
		"channels", 2, global_var->config_file);
	encoder->avg_bytes_per_sec = GetPrivateProfileIntA("audio", 
		"avg_bytes_per_sec", 48000, global_var->config_file);

	av_register_all();

	//申请输出格式上下文
	ret = avformat_alloc_output_context2(&encoder->fmt_ctx, NULL, NULL, global_var->record_file);
	if (ret < 0)
	{
		ret = -1;
		goto failed;
	}

	//初始化视频流
	encoder->video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!encoder->video_codec)
	{
		ret = -2;
		goto failed;
	}
	encoder->video_stream = avformat_new_stream(encoder->fmt_ctx, encoder->video_codec);//如果指定编码器，那么也会初始化AVCodecContext的私有部分，否则只初始化通用部分
	if (NULL == encoder->video_stream)
	{
		ret = -3;
		goto failed;
	}
	encoder->video_stream->id = 0;
	encoder->video_stream->time_base.num = 1;
	encoder->video_stream->time_base.den = encoder->fps;
	encoder->video_codec_ctx = encoder->video_stream->codec;
	encoder->video_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	encoder->video_codec_ctx->width = encoder->width;
	encoder->video_codec_ctx->height = encoder->height;
	encoder->video_codec_ctx->sample_aspect_ratio.num = 0;//看注释是长宽比，不知道的情况下可以填0
	encoder->video_codec_ctx->sample_aspect_ratio.den = 0;
	encoder->video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;//视频格式 pix_fmts[0] 就是yuv420p//video_codec_ctx->pix_fmt = video_codec->pix_fmts[0];
	encoder->video_codec_ctx->time_base.num = 1;	//10fps
	encoder->video_codec_ctx->time_base.den = encoder->fps;
	encoder->video_codec_ctx->gop_size = 10;//组的大小，IDR+n b + n p 等于一组
	encoder->video_codec_ctx->max_b_frames = 1;
	encoder->video_codec_ctx->me_range = 16;
	encoder->video_codec_ctx->bit_rate = encoder->bit_rate;//码率
	encoder->video_codec_ctx->max_qdiff = 3;
	encoder->video_codec_ctx->qmin = 10;
	encoder->video_codec_ctx->qmax = 20;//取值可能是0-51 越接近51，视频质量越模糊
	encoder->video_codec_ctx->qcompress = 0.6;
	ret = av_opt_set(encoder->video_codec_ctx->priv_data, "preset", "superfast", 0);//编码速度快
	if (ret < 0)
	{
		ret = -4;
		goto failed;
	}
	ret = av_opt_set(encoder->video_codec_ctx->priv_data, "tune", "zerolatency", 0);//不延时，不在编码器内缓冲帧
	if (ret < 0)
	{
		ret = -5;
		goto failed;
	}
	ret = avcodec_open2(encoder->video_codec_ctx, encoder->video_codec, NULL);//打开视频编码器
	if (ret < 0)
	{
		ret = -6;
		goto failed;
	}
	if (encoder->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		encoder->video_codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	//初始化音频流
	encoder->audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);//查找编码器
	if (NULL == encoder->audio_codec)
	{
		ret = -7;
		goto failed;
	}
	encoder->audio_stream = avformat_new_stream(encoder->fmt_ctx, encoder->audio_codec);
	if (NULL == encoder->audio_stream)
	{
		ret = -8;
		goto failed;
	}
	encoder->audio_stream->id = 1;
	encoder->audio_stream->time_base.num = 1;
	encoder->audio_stream->time_base.den = encoder->samples_per_sec;
	encoder->audio_codec_ctx = encoder->audio_stream->codec;//编码器上下文更容易访问
	encoder->audio_codec_ctx->sample_rate = encoder->samples_per_sec;//初始化音频编码器上下文
	encoder->audio_codec_ctx->channel_layout = av_get_default_channel_layout(encoder->channels);
	encoder->audio_codec_ctx->channels = encoder->channels;
	encoder->audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;//audio_codec_ctx->sample_fmt = audio_codec->sample_fmts[0];
	encoder->audio_codec_ctx->bit_rate = encoder->avg_bytes_per_sec;
	encoder->audio_codec_ctx->time_base.num = 1;
	encoder->audio_codec_ctx->time_base.den = encoder->samples_per_sec;
	encoder->audio_codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
	encoder->audio_codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
	if (encoder->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		encoder->audio_codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
	ret = avcodec_open2(encoder->audio_codec_ctx, encoder->audio_codec, NULL);//打开音频编码器
	if (ret < 0)
	{
		ret = -9;
		goto failed;
	}

	av_dump_format(encoder->fmt_ctx, 0, global_var->record_file, 1);

	if (global_var->record)
	{
		//打开文件
		if (!(encoder->fmt_ctx->oformat->flags & AVFMT_NOFILE))
		{
			ret = avio_open(&encoder->fmt_ctx->pb, global_var->record_file, AVIO_FLAG_WRITE);
			if (ret < 0) 
			{
				ret = -10;
				goto failed;
			}
		}

		//容器首部
		ret = avformat_write_header(encoder->fmt_ctx, NULL);
		if (ret < 0)
		{
			ret = -11;
			goto failed;
		}
	}

	//后面需要AVFrame来装载yuv数据，在此做初始化
	encoder->video_frame = av_frame_alloc();
	if (NULL == encoder->video_frame)
	{
		ret = -12;
		goto failed;
	}
	encoder->video_frame->format = encoder->video_codec_ctx->pix_fmt;
	//此处为什么不让frame的宽高等于编码器上下文的宽高？
	//答:因为此处的frame的作用是状态yuv原始数据，当原始数据宽高和编码器上下文的宽高不相等的时候
	//需要进行格式转换，重新申请一个目标frame，用于编码，那时候，此处的frame将作为convert的源frame
	encoder->video_frame->width  = encoder->width;
	encoder->video_frame->height = encoder->height;
	ret = av_frame_get_buffer(encoder->video_frame, 32);
	if (ret < 0)
	{
		ret = -13;
		goto failed;
	}
	encoder->video_frame->linesize[0] = encoder->width;//通过av_frame_get_buffer得到的linesize[0]=1376 [1]=704 [2]=704
	encoder->video_frame->linesize[1] = encoder->width/2;//但是我们的数据是每行Y=1366 U=V=1366/2，所以此处我自己做了修改，有什么影响暂且不知道
	encoder->video_frame->linesize[2] = encoder->width/2;

	//初始化音频帧，用来装载麦克风录入的音频数据
	encoder->audio_frame = av_frame_alloc();
	if (NULL == encoder->audio_frame)
	{
		ret = -14;
		goto failed;
	}
	encoder->audio_frame->nb_samples = encoder->audio_codec_ctx->frame_size;
	encoder->audio_frame->channel_layout = encoder->audio_codec_ctx->channel_layout;
	encoder->audio_frame->format = encoder->audio_codec_ctx->sample_fmt;
	encoder->audio_frame->sample_rate = encoder->audio_codec_ctx->sample_rate;
	ret = av_frame_get_buffer(encoder->audio_frame, 32);
	if (ret < 0)
	{
		ret = -15;
		goto failed;
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)global_var->log_file, LOG_DEBUG, log_str);

	ret = 0;
	return ret;

failed:
	if (encoder->video_frame)
		av_frame_free(&encoder->video_frame);
	if (encoder->video_codec_ctx)
		avcodec_close(encoder->video_codec_ctx);
	if (encoder->audio_frame)
		av_frame_free(&encoder->audio_frame);
	if (encoder->audio_codec_ctx)
		avcodec_close(encoder->audio_codec_ctx);
	if (encoder->fmt_ctx)
		avformat_free_context(encoder->fmt_ctx);

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