#include <Windows.h>
#include "log.h"
#include "encode.h"

#pragma comment(lib, "log.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "swscale.lib")

void InitEncoderParam(PENCODER pEncoder, PENCODECONFIG pEncodeConfig)
{
	int size = 0, ret = 0;
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	memcpy(pEncoder->record_file, pEncodeConfig->record_file, strlen(pEncodeConfig->record_file));
	pEncoder->record = pEncodeConfig->record;


	pEncoder->fps = pEncodeConfig->fps;
	pEncoder->width = pEncodeConfig->width;
	pEncoder->height = pEncodeConfig->height;
	pEncoder->bit_rate = pEncodeConfig->bit_rate;
	pEncoder->video_pts = 0;

	pEncoder->channels = pEncodeConfig->channels;
	pEncoder->bits_per_sample = pEncodeConfig->bits_per_sample;
	pEncoder->sample_rate = pEncodeConfig->sample_rate;
	pEncoder->avg_bytes_per_sec = pEncodeConfig->avg_bytes_per_sec;
	pEncoder->audio_pts = 0;

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
}

int CheckParam(PENCODECONFIG pEncodeConfig)
{
	int a = pEncodeConfig->avg_bytes_per_sec;
	if (NULL == pEncodeConfig||
		pEncodeConfig->fps <= 0 ||
		pEncodeConfig->width <= 0 ||
		pEncodeConfig->height <= 0 ||
		pEncodeConfig->bit_rate <= 0 ||
		pEncodeConfig->channels <= 0 ||
		pEncodeConfig->bits_per_sample <= 0 ||
		pEncodeConfig->sample_rate <= 0 ||
		pEncodeConfig->avg_bytes_per_sec <= 0)
	{
		return WRONG_PARAM;
	}
	return SECCESS;
}

void Clean(PENCODER pEncoder)
{
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	if (pEncoder->audio_frame)
	{
		av_frame_free(&pEncoder->audio_frame);
		pEncoder->audio_frame = NULL;
	}

	if (pEncoder->video_frame)
	{
		av_frame_free(&pEncoder->video_frame);
		pEncoder->video_frame = NULL;
	}

	if (pEncoder->video_codec_ctx)
	{
		avcodec_close(pEncoder->video_codec_ctx);
		pEncoder->video_codec_ctx = NULL;
	}

	if (pEncoder->audio_codec_ctx)
	{
		avcodec_close(pEncoder->audio_codec_ctx);
		pEncoder->audio_codec_ctx = NULL;
	}

	if (pEncoder->fmt_ctx)
	{
		avformat_free_context(pEncoder->fmt_ctx);
		pEncoder->fmt_ctx = NULL;
	}

	if (pEncoder)
	{
		free(pEncoder);
	}
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
}

int InitFfmpeg(PENCODER pEncoder)
{
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	av_register_all();

	//申请格式上下文
	if (0 > 
		avformat_alloc_output_context2(&pEncoder->fmt_ctx, NULL, NULL, pEncoder->record_file))
	{
		PRINTLOG(LOG_ERROR, "%s %d avformat_alloc_output_context2 failed\n", __FUNCTION__, __LINE__);
		return AF_ALLOC_OUTPUT;
	}

	//video
	pEncoder->video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (NULL == pEncoder->video_codec)
	{
		PRINTLOG(LOG_ERROR, "%s %d avcodec_find_encoder do not find AV_CODEC_ID_H264 encoder\n",
			__FUNCTION__, __LINE__);
		return NOT_FIND_ENCODER;
	}

	//如果指定编码器，那么也会初始化AVCodecContext的私有部分，否则只初始化通用部分
	pEncoder->video_stream = avformat_new_stream(pEncoder->fmt_ctx, pEncoder->video_codec);
	if (NULL == pEncoder->video_stream)
	{
		PRINTLOG(LOG_ERROR, "%s %d avformat_new_stream new video stream failed\n",
			__FUNCTION__, __LINE__);
		return NEW_STREAM_FAILED;
	}
	pEncoder->video_stream->id = VIDEO_STREAM_INDEX;
	pEncoder->video_stream->time_base.num = 1;
	pEncoder->video_stream->time_base.den = 90000;//fps;

	pEncoder->video_codec_ctx = pEncoder->video_stream->codec;
	pEncoder->video_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	pEncoder->video_codec_ctx->width = pEncoder->width;
	pEncoder->video_codec_ctx->height = pEncoder->height;
	//看注释是长宽比，不知道的情况下可以填0
	pEncoder->video_codec_ctx->sample_aspect_ratio.num = 0;
	pEncoder->video_codec_ctx->sample_aspect_ratio.den = 0;
	//视频格式 pix_fmts[0] 就是yuv420p//video_codec_ctx->pix_fmt = video_codec->pix_fmts[0];
	pEncoder->video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	pEncoder->video_codec_ctx->time_base.num = 1;
	pEncoder->video_codec_ctx->time_base.den = pEncoder->fps;
	pEncoder->video_codec_ctx->ticks_per_frame = 2;
	//组的大小，IDR+n b + n p 等于一组
	pEncoder->video_codec_ctx->gop_size = 0;
	pEncoder->video_codec_ctx->max_b_frames = 1;
	pEncoder->video_codec_ctx->me_range = 16;
	pEncoder->video_codec_ctx->bit_rate = pEncoder->bit_rate;//码率
	pEncoder->video_codec_ctx->max_qdiff = 3;
	pEncoder->video_codec_ctx->qmin = 18;
	pEncoder->video_codec_ctx->qmax = 20;//取值可能是0-51 越接近51，视频质量越模糊
	pEncoder->video_codec_ctx->qcompress = 0.6;

	//编码速度快 slower superfast
	if (0 > 
		av_opt_set(pEncoder->video_codec_ctx->priv_data, "preset", "slower", 0))
	{
		PRINTLOG(LOG_ERROR, "%s %d av_opt_set set video codec context's priv_data preset to superfast failed\n",
			__FUNCTION__, __LINE__);
		return SET_OPT_FAILED;
	}

	//crf

	if (0 > 
		av_opt_set(pEncoder->video_codec_ctx->priv_data, "crf", "18", 0))
	{
		PRINTLOG(LOG_ERROR, "%s %d av_opt_set set video codec context's priv_data crf to 18 failed\n",
			__FUNCTION__, __LINE__);
		return SET_OPT_FAILED;
	}

	//qp
	if(0 > av_opt_set(pEncoder->video_codec_ctx->priv_data, "qp", "0", 0))
	{
		PRINTLOG(LOG_ERROR, "%s %d av_opt_set set video codec context's priv_data qp to 0 failed\n",
			__FUNCTION__, __LINE__);
		return SET_OPT_FAILED;
	}
	
	//不延时，不在编码器内缓冲帧
	if (0 > 
		av_opt_set(pEncoder->video_codec_ctx->priv_data, "tune", "zerolatency", 0))
	{
		PRINTLOG(LOG_ERROR, "%s %d av_opt_set set video codec context's priv_data tune to zerolatency failed\n",
			__FUNCTION__, __LINE__);
		return SET_OPT_FAILED;
	}

	//打开视频编码器
	if (0 > 
		avcodec_open2(pEncoder->video_codec_ctx, pEncoder->video_codec, NULL))
	{
		PRINTLOG(LOG_ERROR, "%s %d avcodec_open2 open video codec failed\n",
			__FUNCTION__, __LINE__);
		return OPEN_CODEC_FAILED;
	}

	if (pEncoder->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		pEncoder->video_codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	pEncoder->audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (NULL == pEncoder->audio_codec)
	{
		PRINTLOG(LOG_ERROR, "%s %d avcodec_find_encoder do not find AV_CODEC_ID_AAC encoder\n",
			__FUNCTION__, __LINE__);
		return NOT_FIND_ENCODER;
	}


	pEncoder->audio_stream = avformat_new_stream(pEncoder->fmt_ctx, pEncoder->audio_codec);
	if (NULL == pEncoder->audio_stream)
	{
		PRINTLOG(LOG_ERROR, "%s %d avformat_new_stream new audio stream failed\n",
			__FUNCTION__, __LINE__);
		return NEW_STREAM_FAILED;
	}

	pEncoder->audio_stream->id = AUDIO_STREAM_INDEX;
	pEncoder->audio_stream->time_base.num = 1;
	pEncoder->audio_stream->time_base.den = pEncoder->sample_rate;

	pEncoder->audio_codec_ctx = pEncoder->audio_stream->codec;
	pEncoder->audio_codec_ctx->sample_rate = pEncoder->sample_rate;
	pEncoder->audio_codec_ctx->channel_layout = av_get_default_channel_layout(pEncoder->channels);
	pEncoder->audio_codec_ctx->channels = pEncoder->channels;
	pEncoder->audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;//audio_codec_ctx->sample_fmt = audio_codec->sample_fmts[0];
	pEncoder->audio_codec_ctx->bit_rate = pEncoder->avg_bytes_per_sec;
	pEncoder->audio_codec_ctx->time_base.num = 1;
	pEncoder->audio_codec_ctx->time_base.den = pEncoder->sample_rate;
	pEncoder->audio_codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
	pEncoder->audio_codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	if (pEncoder->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		pEncoder->audio_codec_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	if (0 > 
		avcodec_open2(pEncoder->audio_codec_ctx, pEncoder->audio_codec, NULL))
	{
		PRINTLOG(LOG_ERROR, "%s %d avcodec_open2 open audio codec failed\n",
			__FUNCTION__, __LINE__);
		return OPEN_CODEC_FAILED;
	}

	//dump
	av_dump_format(pEncoder->fmt_ctx, 0, pEncoder->record_file, 1);

	if (0 != pEncoder->record)
	{
		if (!(pEncoder->fmt_ctx->oformat->flags & AVFMT_NOFILE))
		{
			if (0 > 
				avio_open(&pEncoder->fmt_ctx->pb, pEncoder->record_file, AVIO_FLAG_WRITE))
			{
				PRINTLOG(LOG_ERROR, "%s %d avio_open open output file failed\n",
					__FUNCTION__, __LINE__);
				return OPEN_OUTPUT_FILE_FAILED;
			}
		}
		if (0 > 
			avformat_write_header(pEncoder->fmt_ctx, NULL))
		{
			PRINTLOG(LOG_ERROR, "%s %d avformat_write_header failed\n",
				__FUNCTION__, __LINE__);
			return WRITE_HEADER_FAILED;
		}
	}

	pEncoder->video_frame = av_frame_alloc();
	if (NULL == pEncoder->video_frame)
	{
		PRINTLOG(LOG_ERROR, "%s %d av_frame_alloc alloc video frame failed\n",
			__FUNCTION__, __LINE__);
		return ALLOC_FRAME_FAILED;
	}

	pEncoder->video_frame->format = pEncoder->video_codec_ctx->pix_fmt;
	pEncoder->video_frame->width  = pEncoder->width;
	pEncoder->video_frame->height = pEncoder->height;

	if(0 < 
		av_frame_get_buffer(pEncoder->video_frame, 32))
	{
		PRINTLOG(LOG_ERROR, "%s %d av_frame_get_buffer video frame buffer failed\n",
			__FUNCTION__, __LINE__);
		return GET_FRAME_BUFFER_FAILED;
	}

	pEncoder->video_frame->linesize[0] = pEncoder->width;
	pEncoder->video_frame->linesize[1] = pEncoder->width/2;
	pEncoder->video_frame->linesize[2] = pEncoder->width/2;

	pEncoder->audio_frame = av_frame_alloc();
	if (NULL == pEncoder->audio_frame)
	{
		PRINTLOG(LOG_ERROR, "%s %d av_frame_alloc alloc audio frame failed\n",
			__FUNCTION__, __LINE__);
		return ALLOC_FRAME_FAILED;
	}

	pEncoder->audio_frame->nb_samples = pEncoder->audio_codec_ctx->frame_size;
	pEncoder->audio_frame->channel_layout = pEncoder->audio_codec_ctx->channel_layout;
	pEncoder->audio_frame->format = pEncoder->audio_codec_ctx->sample_fmt;
	pEncoder->audio_frame->sample_rate = pEncoder->audio_codec_ctx->sample_rate;
	if (0 > 
		av_frame_get_buffer(pEncoder->audio_frame, 32))
	{
		PRINTLOG(LOG_ERROR, "%s %d av_frame_get_buffer audio frame buffer failed\n",
			__FUNCTION__, __LINE__);
		return GET_FRAME_BUFFER_FAILED;
	}

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return SECCESS;
}

PENCODER InitEncoder(PENCODECONFIG pEncodeConfig)
{
	PENCODER pEncoder = NULL;
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (SECCESS != CheckParam(pEncodeConfig))
	{
		PRINTLOG(LOG_ERROR, "%s %d pEncodeConfig param error\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	pEncoder = (PENCODER)malloc(sizeof(ENCODER));
	if (NULL == pEncoder)
	{
		PRINTLOG(LOG_ERROR, "%s %d pEncoder malloc failed\n", __FUNCTION__, __LINE__);
		return NULL;
	}
	PRINTLOG(LOG_DEBUG, "pEncoder malloc %d\n", sizeof(ENCODER));
	memset(pEncoder, 0, sizeof(ENCODER));

	InitEncoderParam(pEncoder, pEncodeConfig);

	if (SECCESS != InitFfmpeg(pEncoder))
	{
		PRINTLOG(LOG_ERROR, "%s %d InitFfmpeg failed\n", __FUNCTION__, __LINE__);
		Clean(pEncoder);
		return NULL;
	}
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return pEncoder;
}

void FreeFrameAndSwsContext(AVFrame *frame, struct SwsContext *sws_ctx)
{
	if (frame)
		av_frame_free(&frame);
	if (sws_ctx)
		sws_freeContext(sws_ctx);
}

int EncodeVideo(PENCODER pEncoder, void *source, int source_width, int source_height,
	void **dest, unsigned long *dest_size, long long *pts, long long *dts)
{
	int ret = 0;
	AVFrame *frame = NULL;
	struct SwsContext *sws_ctx = NULL;
	int got_frame;

	if (!pEncoder || !source || !dest || !dest_size || !pts || !dts)
	{
		PRINTLOG(LOG_ERROR, "%s %d Wrong param\n", __FUNCTION__, __LINE__);
		return WRONG_PARAM;
	}

	//生成source frame
	frame = av_frame_alloc();
	if (NULL == frame)
	{
		PRINTLOG(LOG_ERROR, "%s %d av_frame_alloc failed\n", __FUNCTION__, __LINE__);
		return ALLOC_FRAME_FAILED;
	}

	frame->format = pEncoder->video_codec_ctx->pix_fmt;
	frame->width  = source_width;
	frame->height = source_height;
	ret = av_frame_get_buffer(frame, 32);
	if (ret < 0)
	{
		PRINTLOG(LOG_ERROR, "%s %d av_frame_alloc failed\n", __FUNCTION__, __LINE__);
		FreeFrameAndSwsContext(frame, sws_ctx);
		return GET_FRAME_BUFFER_FAILED;
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
	sws_ctx = sws_getContext(source_width, source_height, pEncoder->video_codec_ctx->pix_fmt,
		pEncoder->width, pEncoder->height, pEncoder->video_codec_ctx->pix_fmt,
		SWS_BILINEAR, NULL, NULL, NULL);
	if (!sws_ctx)
	{
		PRINTLOG(LOG_ERROR, "%s %d sws_getContext failed\n", __FUNCTION__, __LINE__);
		FreeFrameAndSwsContext(frame, sws_ctx);
		return SWS_GET_CONTEXT_FAILED;
	}

	//转换
	ret = sws_scale(sws_ctx, (const uint8_t * const*)frame->data,
						frame->linesize, 0, source_height, 
						pEncoder->video_frame->data, pEncoder->video_frame->linesize);
	pEncoder->video_frame->pts = pEncoder->video_pts++;
	if (ret < 0)
	{
		PRINTLOG(LOG_ERROR, "%s %d sws_scale failed\n", __FUNCTION__, __LINE__);
		FreeFrameAndSwsContext(frame, sws_ctx);
		return SWS_SCALE_FAILED;
	}

	//初始化packet
	av_init_packet(&pEncoder->video_packet);
	pEncoder->video_packet.data = NULL;
	pEncoder->video_packet.size = 0;

	ret = avcodec_encode_video2(pEncoder->video_codec_ctx, &pEncoder->video_packet, 
								pEncoder->video_frame, 
								//frame,
								&got_frame);
	if (ret < 0)
	{
		PRINTLOG(LOG_ERROR, "%s %d avcodec_encode_video2 failed\n", __FUNCTION__, __LINE__);
		FreeFrameAndSwsContext(frame, sws_ctx);
		return ENCODE_VIDEO_FRAME_FAILED;
	}

	if (got_frame)
	{
		pEncoder->video_packet.stream_index = pEncoder->video_stream->index;
		av_packet_rescale_ts(&pEncoder->video_packet, pEncoder->video_codec_ctx->time_base,
								pEncoder->video_stream->time_base);
		if (pEncoder->record)
		{
			ret = av_write_frame(pEncoder->fmt_ctx, &pEncoder->video_packet);
			if (ret < 0)
			{
				PRINTLOG(LOG_ERROR, "%s %d av_write_frame failed\n", __FUNCTION__, __LINE__);
				FreeFrameAndSwsContext(frame, sws_ctx);
				return WRITE_FRAME_FAILED;
			}
		}

		*dest_size = pEncoder->video_packet.size;
		*pts = (float)pEncoder->video_packet.pts / pEncoder->video_stream->time_base.den * 90000;
		//*pts = encoder->video_packet.pts;
		//*dts = encoder->video_packet.dts;
		*dest = (void *)malloc(*dest_size);
		if (!dest)
		{
			PRINTLOG(LOG_ERROR, "%s %d malloc *dest failed\n", __FUNCTION__, __LINE__);
			FreeFrameAndSwsContext(frame, sws_ctx);
			return MALLOCFAILED;
		}
		memcpy(*dest, pEncoder->video_packet.data, *dest_size);
		FreeFrameAndSwsContext(frame, sws_ctx);
		return SECCESS;
	}

	return NOT_GET_PACKET;
}

int EncodeAudio(PENCODER pEncoder, void *source, unsigned long source_size, 
						PAUDIOPACKET pAudioPacket, int *ap_len)
{
	int ret = 0;
	uint8_t *pcm = (uint8_t *)source;
	long pcm_len = source_size;
	int channels = pEncoder->channels;
	int byte_per_sample = pEncoder->bits_per_sample / 8;
	int got_frame = 0;
	int len = 0;

	if (NULL == pEncoder ||
		NULL == source)
	{
		PRINTLOG(LOG_ERROR, "%s %d Wrong param\n", __FUNCTION__, __LINE__);
		return WRONG_PARAM;
	}

	while (pcm_len > 0)
	{
		pEncoder->audio_frame->nb_samples = FFMIN(pEncoder->audio_codec_ctx->frame_size, pcm_len);
		av_init_packet(&pEncoder->audio_packet);
		pEncoder->audio_packet.data = NULL;
		pEncoder->audio_packet.size = 0;

		pEncoder->audio_frame->pts = pEncoder->audio_pts;
		pEncoder->audio_pts += pEncoder->audio_frame->nb_samples;

		memcpy(pEncoder->audio_frame->data[0], pcm, 
					pEncoder->audio_frame->nb_samples * channels * byte_per_sample);
		pcm += pEncoder->audio_frame->nb_samples * channels * byte_per_sample;

		ret = avcodec_encode_audio2(pEncoder->audio_codec_ctx, &pEncoder->audio_packet,
									pEncoder->audio_frame, &got_frame);
		if (ret < 0)
		{
			PRINTLOG(LOG_ERROR, "%s %d Wrong param\n", __FUNCTION__, __LINE__);
			return ENCODE_AUDIO_FRAME_FAILED;
		}

		if (got_frame)
		{
			pEncoder->audio_packet.stream_index = pEncoder->audio_stream->index;
			av_packet_rescale_ts(&pEncoder->audio_packet, 
									pEncoder->audio_codec_ctx->time_base,
									pEncoder->audio_stream->time_base);
			if (pEncoder->record)
			{
				ret = av_write_frame(pEncoder->fmt_ctx, &pEncoder->audio_packet);
				if (ret < 0)
				{
					PRINTLOG(LOG_ERROR, "%s %d av_write_frame failed\n", __FUNCTION__, __LINE__);
					return WRITE_FRAME_FAILED;
				}
			}

			len = *ap_len;

			pAudioPacket[len].size = pEncoder->audio_packet.size;
			pAudioPacket[len].pts = pEncoder->audio_packet.pts;
			pAudioPacket[len].dts = pEncoder->audio_packet.dts;
			pAudioPacket[len].data = (void *)malloc(pAudioPacket[*ap_len].size);
			if (NULL == pAudioPacket[len].data)
			{
				PRINTLOG(LOG_ERROR, "%s %d malloc *dest failed\n", __FUNCTION__, __LINE__);
				return MALLOCFAILED;
			}
			if (pEncoder->fmt_ctx->streams[AUDIO_STREAM_INDEX]->codec->extradata_size == 0)
			{
				pAudioPacket[len].size -= 7;
				memcpy(pAudioPacket[len].data, pEncoder->audio_packet.data + 7, pAudioPacket[len].size);
			}
			else
			{
				memcpy(pAudioPacket[len].data, pEncoder->audio_packet.data, pAudioPacket[len].size);
			}
			

			(*ap_len)++;
		}
		pcm_len -= pEncoder->audio_frame->nb_samples * channels * byte_per_sample;
	}

	return SECCESS;
}

int FflushEncoder(PENCODER pEncoder)
{
	int got_frame;
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (NULL == pEncoder)
	{
		PRINTLOG(LOG_ERROR, "%s %d\n pEncoder is NULL", __FUNCTION__, __LINE__);
		return WRONG_PARAM;
	}

	//刷新视频编码器内部的延时帧数据
	while (1)
	{
		av_init_packet(&pEncoder->video_packet);
		pEncoder->video_packet.data = NULL;
		pEncoder->video_packet.size = 0;
		if (0 > avcodec_encode_video2(pEncoder->video_codec_ctx, &pEncoder->video_packet, NULL, &got_frame))
			break;

		if (got_frame)
		{
			pEncoder->video_packet.stream_index = pEncoder->video_stream->index;
			av_packet_rescale_ts(&pEncoder->video_packet, pEncoder->video_codec_ctx->time_base,
									pEncoder->video_stream->time_base);
			if (pEncoder->record)
			{
				if (0 > av_write_frame(pEncoder->fmt_ctx, &pEncoder->video_packet))
				{
					PRINTLOG(LOG_ERROR, "%s %d\n av_write_frame video failed", __FUNCTION__, __LINE__);
					return WRITE_FRAME_FAILED;
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
		av_init_packet(&pEncoder->audio_packet);
		pEncoder->audio_packet.data = NULL;
		pEncoder->audio_packet.size = 0;
		if (0 > avcodec_encode_audio2(pEncoder->audio_codec_ctx, &pEncoder->audio_packet, NULL, &got_frame))
			break;

		if (got_frame)
		{
			pEncoder->audio_packet.stream_index = pEncoder->audio_stream->index;
			av_packet_rescale_ts(&pEncoder->audio_packet, pEncoder->audio_codec_ctx->time_base,
									pEncoder->audio_stream->time_base);
			if (pEncoder->record)
			{
				if (0 >av_write_frame(pEncoder->fmt_ctx, &pEncoder->audio_packet))
				{
					PRINTLOG(LOG_ERROR, "%s %d\n av_write_frame audio failed", __FUNCTION__, __LINE__);
					return WRITE_FRAME_FAILED;
				}
			}
		}
		else
		{
			break;
		}
	}

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return SECCESS;
}

int FreeEncoder(PENCODER pEncoder)
{
	int ret = 0;

	if (NULL == pEncoder)
	{
		PRINTLOG(LOG_ERROR, "%s %d\n pEncoder is NULL", __FUNCTION__, __LINE__);
		return WRONG_PARAM;
	}

	if (pEncoder->record)
	{
		av_write_trailer(pEncoder->fmt_ctx);
	}

	Clean(pEncoder);
	return SECCESS;
}
