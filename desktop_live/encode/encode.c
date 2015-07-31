#include "log.h"
#include "encode.h"


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

#pragma comment(lib, "log.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "swscale.lib")

typedef struct 
{
	void *log;
	char config_file[MAX_PATH];
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
}ENCODER;

static ENCODER *s_encoder;

int init_encoder_param(ENCODER *encoder, void *log, char *config_file, char *record_file, int record)
{
	int size = 0, ret = 0;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)log, LOG_DEBUG, log_str);

	encoder->log = log;

	size = strlen(config_file)+1;
	memset(encoder->config_file, 0, size);
	memcpy(encoder->config_file, config_file, size-1);
	encoder->config_file[size] = '\0';

	size = strlen(record_file)+1;
	memset(encoder->record_file, 0, size);
	memcpy(encoder->record_file, record_file, size-1);
	encoder->record_file[size] = '\0';

	encoder->record = record;

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)log, LOG_DEBUG, log_str);
	ret = 0;
	return ret;
}

int init_encoder_video_param(ENCODER *encoder)
{
	char *config_file = encoder->config_file;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);

	
	encoder->fps = GetPrivateProfileIntA("encode", "fps", 10, config_file);
	encoder->width = GetPrivateProfileIntA("encode", "width", 1366, config_file);
	encoder->height = GetPrivateProfileIntA("encode", "height", 768, config_file);
	encoder->bit_rate = GetPrivateProfileIntA("encode", "bit_rate", 400000, config_file);
	encoder->video_pts = 0;

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	return 0;
}

int init_encoder_audio_param(ENCODER *encoder)
{
	char *config_file = encoder->config_file;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);

	encoder->channels = GetPrivateProfileIntA("encode", "channels", 2, config_file);
	encoder->bits_per_sample = GetPrivateProfileIntA("encode", "bits_per_sample", 16, config_file);
	encoder->sample_rate = GetPrivateProfileIntA("encode", "sample_rate", 48000, config_file);
	encoder->avg_bytes_per_sec = GetPrivateProfileIntA("encode", "avg_bytes_per_sec", 48000, config_file);
	encoder->audio_pts = 0;

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	return 0;
}

int init_format_context(ENCODER *encoder)
{
	int ret = 0;
	AVFormatContext *fmt_ctx = NULL;
	char *record_file = encoder->record_file;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);

	av_register_all();

	//申请格式上下文
	avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, record_file);
	if (!fmt_ctx)
	{
		ret = -1;
		return ret;
	}

	encoder->fmt_ctx = fmt_ctx;

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	ret = 0;
	return ret;
}

int init_video_stream(ENCODER *encoder)
{
	int ret = 0;
	AVStream *stream = NULL;
	AVCodecContext *codec_ctx = NULL;
	AVCodec *codec = NULL;
	int fps = encoder->fps;
	int width = encoder->width;
	int height = encoder->height;
	int bit_rate = encoder->bit_rate;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);


	codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!codec)
	{
		ret = -1;
		return ret;
	}

	//如果指定编码器，那么也会初始化AVCodecContext的私有部分，否则只初始化通用部分
	stream = avformat_new_stream(encoder->fmt_ctx, codec);
	if (!stream)
	{
		ret = -2;
		return ret;
	}

	stream->id = VIDEO_STREAM_INDEX;
	stream->time_base.num = 1;
	stream->time_base.den = fps;
	codec_ctx = stream->codec;
	codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	codec_ctx->width = width;
	codec_ctx->height = height;

	//看注释是长宽比，不知道的情况下可以填0
	codec_ctx->sample_aspect_ratio.num = 0;
	codec_ctx->sample_aspect_ratio.den = 0;

	//视频格式 pix_fmts[0] 就是yuv420p//video_codec_ctx->pix_fmt = video_codec->pix_fmts[0];
	codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	codec_ctx->time_base.num = 1;
	codec_ctx->time_base.den = fps;
	
	//组的大小，IDR+n b + n p 等于一组
	codec_ctx->gop_size = 0;
	codec_ctx->max_b_frames = 1;
	codec_ctx->me_range = 16;
	codec_ctx->bit_rate = bit_rate;//码率
	codec_ctx->max_qdiff = 3;
	codec_ctx->qmin = 10;
	codec_ctx->qmax = 20;//取值可能是0-51 越接近51，视频质量越模糊
	codec_ctx->qcompress = 0.6;

	//编码速度快
	ret = av_opt_set(codec_ctx->priv_data, "preset", "superfast", 0);
	if (ret < 0)
	{
		ret = -3;
		return ret;
	}
	
	//不延时，不在编码器内缓冲帧
	ret = av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);
	if (ret < 0)
	{
		ret = -4;
		return ret;
	}

	//打开视频编码器
	ret = avcodec_open2(codec_ctx, codec, NULL);
	if (ret < 0)
	{
		ret = -5;
		return ret;
	}

	if (encoder->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	encoder->video_stream = stream;
	encoder->video_codec_ctx = codec_ctx;
	encoder->video_codec = codec;

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	ret = 0;
	return ret;
}

int init_audio_stream(ENCODER *encoder)
{
	int ret = 0;
	AVCodec *codec = NULL;
	AVStream *stream = NULL;
	AVCodecContext *codec_ctx = NULL;
	int sample_rate = encoder->sample_rate;
	int channels = encoder->channels;
	int avg_bytes_per_sec = encoder->avg_bytes_per_sec;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);

	//查找编码器
	codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!codec)
	{
		ret = -1;
		return ret;
	}

	stream = avformat_new_stream(encoder->fmt_ctx, codec);
	if (!stream)
	{
		ret = -2;
		return ret;
	}

	stream->id = AUDIO_STREAM_INDEX;
	stream->time_base.num = 1;
	stream->time_base.den = sample_rate;

	codec_ctx = stream->codec;
	codec_ctx->sample_rate = sample_rate;
	codec_ctx->channel_layout = av_get_default_channel_layout(channels);
	codec_ctx->channels = channels;
	codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;//audio_codec_ctx->sample_fmt = audio_codec->sample_fmts[0];
	codec_ctx->bit_rate = avg_bytes_per_sec;
	codec_ctx->time_base.num = 1;
	codec_ctx->time_base.den = sample_rate;
	codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
	codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	if (encoder->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	ret = avcodec_open2(codec_ctx, codec, NULL);
	if (ret < 0)
	{
		ret = -3;
		return ret;
	}

	encoder->audio_stream = stream;
	encoder->audio_codec_ctx = codec_ctx;
	encoder->audio_codec = codec;

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	ret = 0;
	return ret;
}

int init_record_file(ENCODER *encoder)
{
	int ret = 0;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	if (encoder->record)
	{
		//打开文件
		if (!(encoder->fmt_ctx->oformat->flags & AVFMT_NOFILE))
		{
			ret = avio_open(&encoder->fmt_ctx->pb, encoder->record_file, AVIO_FLAG_WRITE);
			if (ret < 0) 
			{
				ret = -1;
				return ret;
			}
		}

		//容器首部
		ret = avformat_write_header(encoder->fmt_ctx, NULL);
		if (ret < 0)
		{
			ret = -2;
			return ret;
		}
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	ret = 0;
	return ret;
}

int init_video_frame(ENCODER *encoder)
{
	int ret = 0;
	AVFrame *frame = NULL;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);

	frame = av_frame_alloc();
	if (!frame)
	{
		ret = -1;
		return ret;
	}

	frame->format = encoder->video_codec_ctx->pix_fmt;
	frame->width  = encoder->width;
	frame->height = encoder->height;
	ret = av_frame_get_buffer(frame, 32);
	if (ret < 0)
	{
		ret = -2;
		return ret;
	}

	frame->linesize[0] = encoder->width;
	frame->linesize[1] = encoder->width/2;
	frame->linesize[2] = encoder->width/2;

	encoder->video_frame = frame;

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	ret = 0;
	return ret;
}

int init_audio_frame(ENCODER *encoder)
{
	int ret = 0;
	AVFrame *frame = NULL;
	AVCodecContext *codec_ctx = encoder->audio_codec_ctx;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	frame = av_frame_alloc();
	if (!frame)
	{
		ret = -1;
		return ret;
	}

	frame->nb_samples = codec_ctx->frame_size;
	frame->channel_layout = codec_ctx->channel_layout;
	frame->format = codec_ctx->sample_fmt;
	frame->sample_rate = codec_ctx->sample_rate;
	ret = av_frame_get_buffer(frame, 32);
	if (ret < 0)
	{
		ret = -2;
		return ret;
	}

	encoder->audio_frame = frame;

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	ret = 0;
	return ret;
}

int init_ercoder(void *log, char *config_file, char *record_file, int record)
{
	int ret;
	ENCODER *encoder = NULL;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)log, LOG_DEBUG, log_str);

	if (s_encoder || !log || !config_file || !record_file)
	{
		ret = -1;
		return ret;
	}

	encoder = (ENCODER *)malloc(sizeof(ENCODER));
	sprintf(log_str, " %s:%d encoder = malloc %d\r\n",__FUNCTION__, __LINE__, sizeof(ENCODER));
	print_log((LOG *)log, LOG_DEBUG, log_str);
	if (!encoder)
	{
		ret = -2;
		goto FAILED;
	}

	ret = init_encoder_param(encoder, log, config_file, record_file, record);
	if (ret != 0)
	{
		ret = -3;
		goto FAILED;
	}

	init_encoder_video_param(encoder);
	init_encoder_audio_param(encoder);

	ret = init_format_context(encoder);
	if (ret != 0)
	{
		ret = -4;
		goto FAILED;
	}

	ret = init_video_stream(encoder);
	if (ret != 0)
	{
		ret = -5;
		return ret;
	}

	ret = init_audio_stream(encoder);
	if (ret != 0)
	{
		ret = -6;
		return ret;
	}

	//dump
	av_dump_format(encoder->fmt_ctx, 0, encoder->record_file, 1);

	init_record_file(encoder);

	ret = init_video_frame(encoder);
	if (ret != 0)
	{
		ret = -7;
		return ret;
	}

	ret = init_audio_frame(encoder);
	if (ret != 0)
	{
		ret = -8;
		return ret;
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	s_encoder = encoder;
	ret = 0;
	return ret;

FAILED:

	if (encoder->audio_frame)
	{
		av_frame_free(&encoder->audio_frame);
		encoder->audio_frame = NULL;
	}

	if (encoder->video_frame)
	{
		av_frame_free(&encoder->video_frame);
		encoder->video_frame = NULL;
	}

	if (encoder->video_codec_ctx)
	{
		avcodec_close(encoder->video_codec_ctx);
		encoder->video_codec_ctx = NULL;
	}

	if (encoder->audio_codec_ctx)
	{
		avcodec_close(encoder->audio_codec_ctx);
		encoder->audio_codec_ctx = NULL;
	}

	if (encoder->fmt_ctx)
	{
		avformat_free_context(encoder->fmt_ctx);
		encoder->fmt_ctx = NULL;
	}

	if (encoder)
	{
		free(encoder);
		s_encoder = NULL;
	}

	return ret;
}

int encode_video(void *source, int source_width, int source_height,
	void **dest, unsigned long *dest_size, long long *pts, long long *dts)
{
	int ret = 0;
	AVFrame *frame = NULL;
	ENCODER *encoder = s_encoder;
	struct SwsContext *sws_ctx = NULL;
	int got_frame;

	if (!encoder || !source || !dest || !dest_size || !pts || !dts)
	{
		ret = -1;
		return ret;
	}

	//生成source frame
	frame = av_frame_alloc();
	if (!frame)
	{
		ret = -2;
		goto FAILED;
	}

	frame->format = encoder->video_codec_ctx->pix_fmt;
	frame->width  = source_width;
	frame->height = source_height;
	ret = av_frame_get_buffer(frame, 32);
	if (ret < 0)
	{
		ret = -2;
		goto FAILED;
	}
	frame->linesize[0] = source_width;
	frame->linesize[1] = source_width/2;
	frame->linesize[2] = source_width/2;

	frame->data[0] = (uint8_t *)source;//亮度Y
	frame->data[1] = (uint8_t *)source + source_width * source_height * 5 / 4;//色度U
	frame->data[2] = (uint8_t *)source + source_width * source_height;//色度V
//	frame->pts = encoder->video_pts++;
//	printf("pts = %d\n", frame->pts);

	//初始化格式转换上下文
	sws_ctx = sws_getContext(source_width, source_height, encoder->video_codec_ctx->pix_fmt,
		encoder->width, encoder->height, encoder->video_codec_ctx->pix_fmt,
		SWS_BILINEAR, NULL, NULL, NULL);
	if (!sws_ctx)
	{
		ret = -3;
		goto FAILED;
	}

	//转换
	ret = sws_scale(sws_ctx, (const uint8_t * const*)frame->data,
						frame->linesize, 0, source_height, 
						encoder->video_frame->data, encoder->video_frame->linesize);
	encoder->video_frame->pts = encoder->video_pts++;
	if (ret < 0)
	{
		ret = -4;
		goto FAILED;
	}

	//初始化packet
	av_init_packet(&encoder->video_packet);
	encoder->video_packet.data = NULL;
	encoder->video_packet.size = 0;

	ret = avcodec_encode_video2(encoder->video_codec_ctx, &encoder->video_packet, 
								encoder->video_frame, 
								//frame,
								&got_frame);
	if (ret < 0)
	{
		ret = -5;
		goto FAILED;
	}
	if (got_frame)
	{
		encoder->video_packet.stream_index = encoder->video_stream->index;
		av_packet_rescale_ts(&encoder->video_packet, encoder->video_codec_ctx->time_base,
								encoder->video_stream->time_base);
		if (encoder->record)
		{
			ret = av_write_frame(encoder->fmt_ctx, &encoder->video_packet);
			if (ret < 0)
			{
				ret = -6;
				goto FAILED;
			}
		}

		*dest_size = encoder->video_packet.size;
		*pts = encoder->video_packet.pts;
		*dts = encoder->video_packet.dts;
		*dest = (void *)malloc(*dest_size);
		if (!dest)
		{
			ret = -7;
			goto FAILED;
		}
		memcpy(*dest, encoder->video_packet.data, *dest_size);
	
		if (frame)
			av_frame_free(&frame);
		if (sws_ctx)
			sws_freeContext(sws_ctx);

		ret = 0;
		return ret;
	}

	ret = -8;

FAILED:
	if (frame)
		av_frame_free(&frame);
	if (sws_ctx)
		sws_freeContext(sws_ctx);

	return ret;

}

int encode_audio(void *source, unsigned long source_size,
					AUDIO_PACKET *ap, int *ap_len)
{
	int ret = 0;
	uint8_t *pcm = (uint8_t *)source;
	long pcm_len = source_size;
	ENCODER *encoder = s_encoder;
	int channels = encoder->channels;
	int byte_per_sample = encoder->bits_per_sample / 8;
	int got_frame = 0;
	int len = 0;

	if (!encoder)
	{
		ret = -1;
		return ret;
	}

	while (pcm_len > 0)
	{
		encoder->audio_frame->nb_samples = FFMIN(encoder->audio_codec_ctx->frame_size, pcm_len);
		av_init_packet(&encoder->audio_packet);
		encoder->audio_packet.data = NULL;
		encoder->audio_packet.size = 0;

		encoder->audio_frame->pts = encoder->audio_pts;
		encoder->audio_pts += encoder->audio_frame->nb_samples;

		memcpy(encoder->audio_frame->data[0], pcm, 
					encoder->audio_frame->nb_samples * channels * byte_per_sample);
		pcm += encoder->audio_frame->nb_samples * channels * byte_per_sample;

		ret = avcodec_encode_audio2(encoder->audio_codec_ctx, &encoder->audio_packet,
									encoder->audio_frame, &got_frame);
		if (ret < 0)
		{
			ret = -2;
			return ret;
		}

		if (got_frame)
		{
			encoder->audio_packet.stream_index = encoder->audio_stream->index;
			av_packet_rescale_ts(&encoder->audio_packet, 
									encoder->audio_codec_ctx->time_base,
									encoder->audio_stream->time_base);
			if (encoder->record)
			{
				ret = av_write_frame(encoder->fmt_ctx, &encoder->audio_packet);
				if (ret < 0)
				{
					ret = -3;
					return ret;
				}
			}

			len = *ap_len;

			ap[len].size = encoder->audio_packet.size;
			ap[len].pts = encoder->audio_packet.pts;
			ap[len].dts = encoder->audio_packet.dts;
			ap[len].data = (void *)malloc(ap[*ap_len].size);
			if (NULL == ap[len].data)
			{
				ret = -4;
				return ret;
			}

			memcpy(ap[len].data, encoder->audio_packet.data, ap[len].size);

			(*ap_len)++;
		}
		pcm_len -= encoder->audio_frame->nb_samples * channels * byte_per_sample;
	}

	ret = 0;
	return ret;
}

int fflush_encoder()
{
	int ret = 0;
	ENCODER *encoder = s_encoder;
	int got_frame;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	if (!encoder)
	{
		ret = -3;
		return ret;
	}

	//刷新视频编码器内部的延时帧数据
	while (1)
	{
		av_init_packet(&encoder->video_packet);
		encoder->video_packet.data = NULL;
		encoder->video_packet.size = 0;
		ret = avcodec_encode_video2(encoder->video_codec_ctx, &encoder->video_packet, NULL, &got_frame);
		if (ret < 0) { break; }	
		if (got_frame)
		{
			encoder->video_packet.stream_index = encoder->video_stream->index;
			av_packet_rescale_ts(&encoder->video_packet, encoder->video_codec_ctx->time_base,
									encoder->video_stream->time_base);
			if (encoder->record)
			{
				ret = av_write_frame(encoder->fmt_ctx, &encoder->video_packet);
				if (ret < 0)
				{
					ret = -1;
					return ret;
				}
			}

		}
		else
		{
			break;
		}
	}

	//刷新音频编码器内部的延时帧数据
	while (1)
	{
		av_init_packet(&encoder->audio_packet);
		encoder->audio_packet.data = NULL;
		encoder->audio_packet.size = 0;
		ret = avcodec_encode_audio2(encoder->audio_codec_ctx, &encoder->audio_packet, NULL, &got_frame);
		if (ret < 0) { break; }	
		if (got_frame)
		{
			encoder->audio_packet.stream_index = encoder->audio_stream->index;
			av_packet_rescale_ts(&encoder->audio_packet, encoder->audio_codec_ctx->time_base,
									encoder->audio_stream->time_base);
			if (encoder->record)
			{
				ret = av_write_frame(encoder->fmt_ctx, &encoder->audio_packet);
				if (ret < 0)
				{
					ret = -2;
					return ret;
				}
			}

		}
		else
		{
			break;
		}
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	ret = 0;
	return ret;
}

int free_encoder()
{
	int ret = 0;
	ENCODER *encoder = s_encoder;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);
	if (!encoder)
	{
		ret = -1;
		return ret;
	}

	if (encoder->record)
	{
		av_write_trailer(encoder->fmt_ctx);
	}

	if (encoder->audio_frame)
	{
		av_frame_free(&encoder->audio_frame);
		encoder->audio_frame = NULL;
	}

	if (encoder->video_frame)
	{
		av_frame_free(&encoder->video_frame);
		encoder->video_frame = NULL;
	}

	if (encoder->video_codec_ctx)
	{
		avcodec_close(encoder->video_codec_ctx);
		encoder->video_codec_ctx = NULL;
	}

	if (encoder->audio_codec_ctx)
	{
		avcodec_close(encoder->audio_codec_ctx);
		encoder->audio_codec_ctx = NULL;
	}

	if (encoder->fmt_ctx)
	{
		avformat_free_context(encoder->fmt_ctx);
		encoder->fmt_ctx = NULL;
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)encoder->log, LOG_DEBUG, log_str);

	if (encoder)
	{
		free(encoder);
		s_encoder = NULL;
	}

	ret = 0;
	return ret;
}