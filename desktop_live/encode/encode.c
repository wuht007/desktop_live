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
#pragma comment(lib, "capture.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")

#ifndef MIN
#  define MIN(a,b)  ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#  define MAX(a,b)  ((a) < (b) ? (b) : (a))
#endif

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
	long count;

	//audio
	AVCodecContext *audio_codec_ctx;
	AVStream *audio_stream;
	AVCodec *audio_codec;
	AVPacket audio_packet;
	AVFrame *audio_frame;
	int samples_per_sec;
	int channels;
	int avg_bytes_per_sec;
	long audio_pts;
}ENCODER;

typedef struct node
{
	uint8_t *data;
	unsigned long size;
	long pts;
	long dts;
	struct list_head list;
}NODE;

typedef struct global_variable
{
#define VIDEO_INDEX 0
#define AUDIO_INDEX 1
	HANDLE handler;
	int stop;
	void *log_file;
	char config_file[MAX_PATH];
	char record_file[MAX_PATH];
	int record;
	RTL_CRITICAL_SECTION cs[2];
	struct list_head head[2];
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

	encoder->count = 0;
	encoder->audio_pts = 0;
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

//返回值 0=成功 其他=失败
//描述:把数据存入队列（一个链表）
//注意:线程不安全，调用前后需加锁解锁
static int enqueue(struct list_head *head, uint8_t *data, unsigned long size, long pts, long dts)
{
	int ret = 0;

	NODE *node = (NODE *)malloc(sizeof(NODE));
	if (NULL == node)
	{
		ret = -1;
		return ret;
	}

	memset(node, 0, sizeof(NODE));
	node->data = (uint8_t *)malloc(size);
	if (NULL == node->data)
	{
		free(node);
		ret = -2;
		return ret;
	}
	memcpy(node->data, data, size);
	node->size = size;
	node->pts = pts;
	node->dts = dts;
	list_add_tail(&node->list, head);
	ret = 0;
	return ret;
}

//返回值 0=成功 其他=失败
//描述:从队列（一个链表）读出数据
//注意:线程不安全，调用前后需加锁解锁
static int dequeue(struct list_head *head, uint8_t **data, unsigned long *size, long *pts, long *dts)
{
	int ret = 0;
	struct list_head *plist = NULL;

	if (0 != list_empty(head))
	{
		ret = -1;
		return ret;
	}

	list_for_each(plist, head) 
	{
		NODE *node = list_entry(plist, struct node, list);
		*data = (uint8_t *)malloc(node->size);
		if (NULL == *data)
		{
			ret = -2;
			return ret;
		}
		*size = node->size;
		*pts = node->pts;
		*dts = node->dts;
		memcpy(*data, node->data, node->size);
		list_del(plist);
		free(node->data);
		free(node);
		break;
	}

	ret = 0;
	return ret;
}

unsigned int __stdcall encode_proc(void *p)
{
	int ret = 0;
	GV *global_var = (GV *)p;
	ENCODER encoder;
	int got_frame = 0;
	DWORD start_time = 0,end_time = 0;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)global_var->log_file, LOG_DEBUG, log_str);


	ret = init_encoder(global_var, &encoder, global_var->log_file);
	if (0 != ret)
	{
		ret = -1;
		return ret;
	}

	while(0 == global_var->stop)
	{
		char *data = NULL;
		unsigned long data_size = 0;
		//延时50毫秒
		start_time = end_time = timeGetTime();
		while(50 < end_time-start_time)
			end_time = timeGetTime();

		//video
		ret = get_video_frame(&data, &data_size);
		if (ret == 0)
		{
			av_init_packet(&encoder.video_packet);
			encoder.video_packet.data = NULL;
			encoder.video_packet.size = 0;

			encoder.video_frame->data[0] = (uint8_t *)data;//亮度Y
			encoder.video_frame->data[1] = (uint8_t *)data + 
												encoder.width * encoder.height * 5 / 4;//色度U
			encoder.video_frame->data[2] = (uint8_t *)data + 
												encoder.width*encoder.height;//色度V
			encoder.video_frame->pts = encoder.count++;
			ret = avcodec_encode_video2(encoder.video_codec_ctx, 
											&encoder.video_packet, encoder.video_frame, &got_frame);
			if (ret < 0)
			{
				ret = -2;
				return -2;
			}
			if (got_frame)
			{
				NODE *node = (NODE *)malloc(sizeof(NODE));
				memset(node, 0, sizeof(NODE));
				encoder.video_packet.stream_index = encoder.video_stream->index;
				av_packet_rescale_ts(&encoder.video_packet , 
										encoder.video_codec_ctx->time_base , 
										encoder.video_stream->time_base);
				if (global_var->record)
				{
					ret = av_write_frame(encoder.fmt_ctx, &encoder.video_packet);
					if (ret < 0)
					{
						ret = -3;
						return ret;
					}
				}

				EnterCriticalSection(&gv->cs[VIDEO_INDEX]);
				ret = enqueue(&global_var->head[VIDEO_INDEX] , 
							encoder.video_packet.data , 
							encoder.video_packet.size ,
							encoder.video_packet.pts ,
							encoder.video_packet.dts);
				LeaveCriticalSection(&gv->cs[VIDEO_INDEX]);
				if (ret < 0)
				{
					ret = -4;
					return ret;
				}
//				send_rtp_video((rtsp_session *)p, (char *)video_packet.data, video_packet.size, video_packet.pts, video_packet.pts);
			}// if (got_frame)
			free(data);
		}//if (0 == ret = get_video_frame(&data, &data_size))

		//audio
		ret = get_audio_frame(&data, &data_size);
		if (ret == 0)
		{
			uint8_t *pcm = (uint8_t *)data;
			int pcm_len = data_size;
			while (pcm_len > 0)
			{
				encoder.audio_frame->nb_samples = MIN(encoder.audio_codec_ctx->frame_size, pcm_len);
				av_init_packet(&encoder.audio_packet);
				encoder.audio_packet.data = NULL;
				encoder.audio_packet.size = 0;

				encoder.audio_frame->pts = encoder.audio_pts;
				encoder.audio_pts += encoder.audio_frame->nb_samples;

				memcpy(encoder.audio_frame->data[0], pcm, encoder.audio_frame->nb_samples*2*2);
				pcm += encoder.audio_frame->nb_samples*2*2;

				ret = avcodec_encode_audio2(encoder.audio_codec_ctx, 
												&encoder.audio_packet,encoder.audio_frame, &got_frame);
				if (ret < 0)
				{
					ret = -5;
					return ret;
				}
				if (got_frame)
				{
					encoder.audio_packet.stream_index = encoder.audio_stream->index;
					av_packet_rescale_ts(&encoder.audio_packet, 
											encoder.audio_codec_ctx->time_base,
											encoder.audio_stream->time_base);
					if (global_var->record)
					{
						ret = av_write_frame(encoder.fmt_ctx, &encoder.audio_packet);
						if (ret < 0)
						{
							ret = -6;
							return ret;
						}
					}

					EnterCriticalSection(&gv->cs[AUDIO_INDEX]);
					ret = enqueue(&global_var->head[AUDIO_INDEX] , 
						encoder.audio_packet.data , 
						encoder.audio_packet.size ,
						encoder.audio_packet.pts ,
						encoder.audio_packet.dts);
					LeaveCriticalSection(&gv->cs[AUDIO_INDEX]);
					if (ret < 0)
					{
						ret = -7;
						return ret;
					}
				}
				pcm_len -= encoder.audio_frame->nb_samples*2*2;
			}//while (pcm_len > 0)
			free(data);
		}// if (0 == ret = get_audio_frame(&data, &data_size))
	}

	//刷新
	
	if (global_var->record)
	{
		av_write_trailer(encoder.fmt_ctx);
	}



	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)global_var->log_file, LOG_DEBUG, log_str);

	if (encoder.video_frame)
		av_frame_free(&encoder.video_frame);
	if (encoder.video_codec_ctx)
		avcodec_close(encoder.video_codec_ctx);
	if (encoder.audio_frame)
		av_frame_free(&encoder.audio_frame);
	if (encoder.audio_codec_ctx)
		avcodec_close(encoder.audio_codec_ctx);
	if (encoder.fmt_ctx)
		avformat_free_context(encoder.fmt_ctx);

	ret = 0;
	return ret;
}

//返回值 0=成功 其他=失败
//描述:开始编码
int start_encode(void *log_file, char *config_file, char *record_file, int record)
{
	int ret = 0, i = 0;
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

	for (i=0; i<2; i++)
	{
		INIT_LIST_HEAD(&gv->head[i]);
		InitializeCriticalSection(&gv->cs[i]);
	}
	
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

int get_video_packet(void **data, unsigned long *size, long *pts, long *dts)
{
	int ret = 0;
	if (NULL == data || NULL == size || NULL == gv || NULL == pts || NULL == dts)
	{
		ret = -1;
		return ret;
	}
	EnterCriticalSection(&gv->cs[VIDEO_INDEX]);
	ret = dequeue(&gv->head[VIDEO_INDEX], (uint8_t **)data, size, pts, dts);
	LeaveCriticalSection(&gv->cs[VIDEO_INDEX]);
	if (ret != 0)
		ret = -2;
	return ret;
}

int get_audio_packet(void **data, unsigned long *size, long *pts, long *dts)
{
	int ret = 0;
	if (NULL == data || NULL == size || NULL == gv || NULL == pts || NULL == dts)
	{
		ret = -1;
		return ret;
	}
	EnterCriticalSection(&gv->cs[VIDEO_INDEX]);
	ret = dequeue(&gv->head[VIDEO_INDEX], (uint8_t **)data, size, pts, dts);
	LeaveCriticalSection(&gv->cs[VIDEO_INDEX]);
	if (ret != 0)
		ret = -2;
	return ret;
}

int stop_encode()
{
	int ret = 0, i = 0;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)gv->log_file, LOG_DEBUG, log_str);
	if (NULL == gv)
	{
		ret = -1;
		return ret;
	}

	gv->stop = 1;

	WaitForSingleObject(gv->handler,INFINITE);

	for (i=0; i<2; i++)
	{
		struct list_head *plist = NULL;

		EnterCriticalSection(&gv->cs[i]);
		while (0 == list_empty(&gv->head[i]))
		{
			list_for_each(plist, &gv->head[i]) 
			{
				NODE *node = list_entry(plist, struct node, list);
				list_del(plist);
				free(node->data);
				free(node);
				break;
			}
		}
		LeaveCriticalSection(&gv->cs[i]);
		DeleteCriticalSection(&gv->cs[i]);
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)gv->log_file, LOG_DEBUG, log_str);

	if (gv)
		free(gv);
	gv = NULL;
	ret = 0;
	return ret;
}