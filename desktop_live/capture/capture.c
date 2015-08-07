#include "capture.h"
#include "list.h"
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <Windows.h>

#ifndef MIN
#  define MIN(a,b)  ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#  define MAX(a,b)  ((a) < (b) ? (b) : (a))
#endif

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "log.lib")

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
}VIDEO;

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
}SCREEN;

typedef struct 
{
	int channels;//2
	int bits_per_sample;//16
	int samples_per_sec;//48000
	int avg_bytes_per_sec;//48000

	unsigned long pcm_len;
	uint8_t *pcm;
}AUDIO;

typedef struct global_variable
{
#define VIDEO_INDEX 0
#define AUDIO_INDEX 1
	HANDLE handler[2];
	int stop;
	LOG *log;
	char config_file[MAX_PATH];
	RTL_CRITICAL_SECTION cs[2];
	struct list_head head[2];
	int width;
	int height;
}GV;

static GV *gv = NULL;

//描述:获取屏幕的宽高，屏幕位图的宽高、深度、通道数、一帧位图的长度
void get_screen_info(SCREEN *screen, LOG *log)
{
	HDC src = NULL;
	HDC mem = NULL;
	HBITMAP bitmap = NULL;
	HBITMAP old_bitmap = NULL;
	BITMAP bmp;

	int left=0;
	int top=0;
	int right=GetSystemMetrics(SM_CXSCREEN);
	int bottom=GetSystemMetrics(SM_CYSCREEN);

	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	screen->width = right - left;
	screen->height = bottom - top;

	src = CreateDC("DISPLAY", NULL, NULL, NULL);
	mem = CreateCompatibleDC(src);
	bitmap = CreateCompatibleBitmap(src, screen->width, screen->height);
	old_bitmap = (HBITMAP)SelectObject(mem, bitmap);
	BitBlt(mem, 0, 0, screen->width, screen->height, src, 0, 0,SRCCOPY);
	bitmap = (HBITMAP)SelectObject(mem, old_bitmap);
	GetObject(bitmap,sizeof(BITMAP),&bmp);

	screen->bitmap_channel = bmp.bmBitsPixel == 1 ? 1 : bmp.bmBitsPixel/8 ;
	screen->bitmap_depth = bmp.bmBitsPixel == 1 ? 1 : 8;//IPL_DEPTH_1U : IPL_DEPTH_8U;
	screen->bitmap_width = bmp.bmWidth;
	screen->bitmap_height = bmp.bmHeight;
	screen->len = screen->bitmap_channel * (screen->bitmap_depth / 8) * \
					screen->bitmap_width * screen->bitmap_height;

	SelectObject(mem,old_bitmap);
	DeleteObject(old_bitmap);
	DeleteDC(mem);
	SelectObject(src,bitmap);
	DeleteDC(mem);
	DeleteObject(bitmap);

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);
}

//返回值:0=成功 其他=失败
//描述:初始化配置文件中的视频参数
int init_capture_video_param(GV *global_var, SCREEN *screen, VIDEO *video, LOG *log)
{
	int ret = -1;

	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	video->fps = GetPrivateProfileIntA("capture", "fps", 10, global_var->config_file);
	video->yuv_len = screen->bitmap_width * \
						screen->bitmap_height*2;
	video->yuv = (uint8_t *)malloc(video->yuv_len);
	if (!video->yuv)
	{
		ret = -1;
		return ret;
	}
	sprintf(log_str, " %s:%d video->yuv = malloc %d\r\n",__FUNCTION__, __LINE__, video->yuv_len);
	print_log(log, LOG_DEBUG, log_str);

	video->rgba_len =  screen->bitmap_width * screen->bitmap_height * \
						screen->bitmap_channel * (screen->bitmap_depth/8);
	video->rgba = (uint8_t *)malloc(video->rgba_len);
	if (!video->rgba)
	{
		ret = -2;
		return ret;
	}
	sprintf(log_str, " %s:%d video->rgba = malloc %d\r\n",__FUNCTION__, __LINE__, video->rgba_len);
	print_log(log, LOG_DEBUG, log_str);

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	ret = 0;
	return ret;
}

//描述:把桌面设备的内存位图复制到video->rgba
void copy_screen_bitmap(SCREEN *screen, VIDEO *video, LOG *log)
{
	HDC src = NULL;
	HDC mem = NULL;
	HBITMAP bitmap = NULL;
	HBITMAP old_bitmap = NULL;

	src = CreateDC("DISPLAY", NULL, NULL, NULL);
	mem = CreateCompatibleDC(src);
	bitmap = CreateCompatibleBitmap(src, screen->width, screen->height);
	old_bitmap = (HBITMAP)SelectObject(mem, bitmap);

	BitBlt(mem, 0, 0, screen->width, screen->height, src, 0, 0 , SRCCOPY);
	bitmap = (HBITMAP)SelectObject(mem, old_bitmap);
	GetBitmapBits(bitmap, screen->len, video->rgba);
	
	SelectObject(mem,old_bitmap);
	DeleteObject(old_bitmap);
	DeleteDC(mem);
	SelectObject(src,bitmap);
	DeleteDC(mem);
	DeleteObject(bitmap);
}

//TRUE=成功 FALSE=失败（算法不会反悔false）
//描述:rgba格式的数据转成yuv420p格式的数据
BOOL RGBA2YUV(LPBYTE RgbaBuf,UINT nWidth,UINT nHeight,LPBYTE yuvBuf,unsigned long *len, int widthStep)
{
	unsigned int i, j;
	unsigned char*bufY, *bufU, *bufV, *bufRGB;
	unsigned char y, u, v, r, g, b, a;
	unsigned int ylen = nWidth * nHeight;
	unsigned int ulen = (nWidth * nHeight)/4;
	unsigned int vlen = (nWidth * nHeight)/4; 
	memset(yuvBuf,0,(unsigned int )*len);
	bufY = yuvBuf;
	bufV = yuvBuf + nWidth * nHeight;
	bufU = bufV + (nWidth * nHeight* 1/4);
	*len = 0; 

	if (widthStep == 0)
		widthStep = nWidth*4;

	for (j = 0; j<nHeight;j++)
	{
		bufRGB = RgbaBuf + widthStep*j;
		for (i = 0;i<nWidth;i++)
		{
			b = *(bufRGB++);
			g = *(bufRGB++);
			r = *(bufRGB++);
			a = *(bufRGB++);//a分量并没有使用

			y = (unsigned char)( ( 66 * r + 129 * g +  25 * b + 128) >> 8) + 16  ;          
			u = (unsigned char)( ( -38 * r -  74 * g + 112 * b + 128) >> 8) + 128 ;          
			v = (unsigned char)( ( 112 * r -  94 * g -  18 * b + 128) >> 8) + 128 ;
			*(bufY++) = MAX( 0, MIN(y, 255 ));
			if (j%2==0&&i%2 ==0)
			{
				if (u>255)
				{
					u=255;
				}
				if (u<0)
				{
					u = 0;
				}
				*(bufU++) =u;
				//存u分量
			}
			else
			{
				//存v分量
				if (i%2==0)
				{
					if (v>255)
					{
						v = 255;
					}
					if (v<0)
					{
						v = 0;
					}
					*(bufV++) =v;
				}
			}
		}
	}
	*len = nWidth * nHeight+(nWidth * nHeight)/2;
	return TRUE;
} 

//返回值 0=成功 其他=失败
//描述:把数据存入队列（一个链表）
//注意:线程不安全，调用前后需加锁解锁
static int enqueue(struct list_head *head, uint8_t *data, unsigned long size)
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
	list_add_tail(&node->list, head);
	ret = 0;
	return ret;
}

//返回值 0=成功 其他=失败
//描述:从队列（一个链表）读出数据
//注意:线程不安全，调用前后需加锁解锁
static int dequeue(struct list_head *head, uint8_t **data, unsigned long *size)
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
		memcpy(*data, node->data, node->size);
		list_del(plist);
		free(node->data);
		free(node);
		break;
	}

	ret = 0;
	return ret;
}

//返回值 0=成功 其他=失败
//描述:采集视频的主逻辑，包括屏幕信息获取，配置文件参数获取，采集转码和入队
unsigned int __stdcall video_capture_proc(void *p)
{
	int ret = 0;
	VIDEO video = {0};
	SCREEN screen = {0};
	unsigned int step_time;
	DWORD start,end;
	GV *global_var = (GV *)p;
	LOG *log = global_var->log;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	get_screen_info(&screen, log);

	global_var->width = screen.bitmap_width;
	global_var->height = screen.bitmap_height;

	ret = init_capture_video_param(global_var, &screen ,&video, log);
	if (ret != 0)
	{
		ret = -1;
		return ret;
	}
	
	step_time = 1000/video.fps;

	while (!global_var->stop)
	{
		start = timeGetTime();
		copy_screen_bitmap(&screen ,&video, log);
		RGBA2YUV((LPBYTE)video.rgba, 
					screen.bitmap_width, 
					screen.bitmap_height, 
					(LPBYTE)video.yuv, 
					&video.yuv_len, 
					screen.bitmap_width*4);

		EnterCriticalSection(&gv->cs[VIDEO_INDEX]);
		ret = enqueue(&global_var->head[VIDEO_INDEX], video.yuv, video.yuv_len);
		LeaveCriticalSection(&gv->cs[VIDEO_INDEX]);
		if (0 != ret)
		{
			ret = -2;
			return ret;
		}

		while((((end = timeGetTime()) - start) < step_time))
			;
	}

	free(video.yuv);
	sprintf(log_str, " %s:%d free video.yuv\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	free(video.rgba);
	sprintf(log_str, " %s:%d free video.rgba\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)log, LOG_DEBUG, log_str);

	return 0;
}

//描述:从配置文件中读出音频采集的基本参数，没读到采用默认值
int init_capture_audio_param(GV *global_var, AUDIO *audio, WAVEFORMATEX *waveformat, LOG *log)
{
	int ret = 0;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	audio->channels = GetPrivateProfileIntA("capture", 
												"channels", 2, global_var->config_file);
	audio->bits_per_sample = GetPrivateProfileIntA("capture", 
														"bits_per_sample", 16, global_var->config_file);
	audio->samples_per_sec = GetPrivateProfileIntA("capture", 
														"samples_per_sec", 48000, global_var->config_file);
	audio->avg_bytes_per_sec = GetPrivateProfileIntA("capture", 
														"avg_bytes_per_sec", 48000, global_var->config_file);

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	ret = 0;
	return 0;
}

//返回值 0=成功 其他=失败
//描述:初始化并且开启wave采集
int start_wave(AUDIO *audio, WAVEFORMATEX *waveformat, 
				WAVEHDR **wavehdr, HWAVEIN *wavein, int HDRCOUNT, LOG *log)
{
	int ret = 0;
	int i = 0;
	int size = 1024*24;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	audio->pcm_len = 0;;
	audio->pcm = (uint8_t *)malloc(size * HDRCOUNT);
	if (NULL == audio->pcm)
	{
		ret = -1;
		return ret;
	}
	sprintf(log_str, " %s:%d audio->pcm = malloc %d\r\n",__FUNCTION__, __LINE__, size * HDRCOUNT);
	print_log(log, LOG_DEBUG, log_str);

	waveformat->wFormatTag = WAVE_FORMAT_PCM;								//采样格式
	waveformat->nChannels = audio->channels;								//声道数
	waveformat->nSamplesPerSec = audio->samples_per_sec;					//采样率
	waveformat->nAvgBytesPerSec = audio->avg_bytes_per_sec;					//平均码率
	waveformat->nBlockAlign = audio->channels * audio->bits_per_sample / 8;	//声道数*每个样本占的字节数
	waveformat->wBitsPerSample = audio->bits_per_sample;					//样本占的位数
	waveformat->cbSize=0;													//特别信息计数

	ret = waveInOpen(wavein, WAVE_MAPPER, waveformat, (DWORD)NULL, 0L, CALLBACK_NULL);
	if (ret != MMSYSERR_NOERROR)
	{
		ret = -2;
		return ret;
	}

	for (i = 0; i < HDRCOUNT; i++)
	{
		wavehdr[i] = (WAVEHDR*)malloc(size + sizeof(WAVEHDR));
		if (NULL == wavehdr[i])
		{
			ret = -3;
			return ret;
		}
		sprintf(log_str, " %s:%d wavehdr[%d] = malloc %d\r\n",__FUNCTION__, __LINE__, i, size + sizeof(WAVEHDR));
		print_log(log, LOG_DEBUG, log_str);

		memset(wavehdr[i], 0, size+sizeof(WAVEHDR));
		wavehdr[i]->dwBufferLength = size;
		wavehdr[i]->lpData = (LPSTR)(wavehdr[i]+1);//(LPSTR)((char *)wavehdr[i]+sizeof(WAVEHDR));

		ret = waveInPrepareHeader(*wavein, wavehdr[i], sizeof(WAVEHDR));
		if (ret != MMSYSERR_NOERROR)
		{
			ret = -4;
			return ret;
		}

		ret = waveInAddBuffer(*wavein, wavehdr[i], sizeof(WAVEHDR));
		if (ret != MMSYSERR_NOERROR)
		{
			ret = -5;
			return ret;
		}
	}

	ret = waveInStart(*wavein);
	if (ret != MMSYSERR_NOERROR)
	{
		ret = -6;
		return ret;
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	ret = 0;
	return ret;
}

//返回值 0=成功 其他=失败
//描述:音频采集主逻辑
unsigned int __stdcall audio_capture_proc(void *p)
{
	int ret = 0, i = 0;
	GV *global_var = (GV *)p;
	AUDIO audio = {0};
	LOG *log = global_var->log;
	
	const int HDRCOUNT = 10;
	int hdr = 0;
	WAVEHDR *wavehdr[10];
	WAVEFORMATEX waveformat;
	HWAVEIN wavein;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	ret = init_capture_audio_param(global_var, &audio, &waveformat, log);
	if (0 != ret)
	{
		ret = -2;
		return ret;
	}

	ret = start_wave(&audio, &waveformat, &wavehdr[0], &wavein, HDRCOUNT, log);
	if (0 != ret)
	{
		ret = -3;
		return ret;
	}


	while (!global_var->stop)
	{
		Sleep(100);
		while (wavehdr[hdr]->dwFlags & WHDR_DONE)
		{
			memcpy(audio.pcm + audio.pcm_len, wavehdr[hdr]->lpData, wavehdr[hdr]->dwBytesRecorded);
			audio.pcm_len += wavehdr[hdr]->dwBytesRecorded;
			ret = waveInAddBuffer(wavein, wavehdr[hdr], sizeof(WAVEHDR));            
			if (ret != MMSYSERR_NOERROR)
			{
				ret = -4;
				return ret;
			}
			hdr = (hdr+1)%HDRCOUNT;
		}

		EnterCriticalSection(&gv->cs[AUDIO_INDEX]);
		ret = enqueue(&gv->head[AUDIO_INDEX], audio.pcm, audio.pcm_len);
		audio.pcm_len = 0;
		LeaveCriticalSection(&gv->cs[AUDIO_INDEX]);
		if (ret != 0)
		{
			ret = -5;
			return ret;
		}
	}

	ret = waveInStop(wavein);
	if (ret != MMSYSERR_NOERROR) 
	{
		ret = -6;
		return ret;
	}

	ret = waveInReset(wavein);
	if (ret != MMSYSERR_NOERROR)
	{
		ret = -7;
		return ret;
	}

	for (i = 0; i < HDRCOUNT; i++)
	{
		ret = waveInUnprepareHeader(wavein, wavehdr[i], sizeof(WAVEHDR));
		if (ret != MMSYSERR_NOERROR)
		{
			ret = -8;
			return ret;
		}
		free(wavehdr[i]);
		sprintf(log_str, " %s:%d free wavehdr[%d]\r\n",__FUNCTION__, __LINE__, i);
		print_log(log, LOG_DEBUG, log_str);
	}

	free(audio.pcm);
	sprintf(log_str, " %s:%d free audio.pcm\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	ret = waveInClose(wavein);
	if (ret != MMSYSERR_NOERROR)
	{
		ret = -9;
		return ret;
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	ret = 0;
	return 0;
}

//返回值 0=成功 其他=失败
//描述:初始化全局变量gv，开启两个线程采集视频和音频
int start_capture(LOG *log, char *config_file)
{
	int ret = 0;
	int i = 0;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

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
	sprintf(log_str, " %s:%d gv = malloc %d\r\n",__FUNCTION__, __LINE__, sizeof(GV));
	print_log((LOG *)log, LOG_DEBUG, log_str);

	memset(gv, 0, sizeof(GV));
	gv->stop = 0;
	gv->log = log;
	memcpy(gv->config_file, config_file, strlen(config_file));

	for (i=0; i<2; i++)
	{
		INIT_LIST_HEAD(&gv->head[i]);
		InitializeCriticalSection(&gv->cs[i]);
		if (VIDEO_INDEX == i)
			gv->handler[i] = (HANDLE)_beginthreadex(NULL, 0, video_capture_proc, gv, 0, NULL);
		else if (AUDIO_INDEX == i)
			gv->handler[i] = (HANDLE)_beginthreadex(NULL, 0, audio_capture_proc, gv, 0, NULL);
		else
			return ret = -3;

		if (NULL == gv->handler[i])
		{
			free(gv);
			gv = NULL;
			ret = -4;
			return ret;
		}
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log(log, LOG_DEBUG, log_str);

	ret = 0;
	return ret;
}

//返回值 0=成功 其他=失败
//描述:获取一帧采集视频数据
int get_video_frame(void **data, unsigned long *size, int *width, int *hetgit)
{
	int ret = 0;
	if (NULL == data || NULL == size || NULL == gv)
	{
		ret = -1;
		return ret;
	}
	EnterCriticalSection(&gv->cs[VIDEO_INDEX]);
	ret = dequeue(&gv->head[VIDEO_INDEX], (uint8_t **)data, size);
	LeaveCriticalSection(&gv->cs[VIDEO_INDEX]);
	if (ret != 0)
		ret = -2;
	*width = gv->width;
	*hetgit = gv->height;
	return ret;
}

//返回值 0=成功 其他=失败
//描述:获取一帧采集音频数据
int get_audio_frame(void **data, unsigned long *size)
{
	int ret = 0;
	if (NULL == data || NULL == size || NULL == gv)
	{
		ret = -1;
		return ret;
	}
	EnterCriticalSection(&gv->cs[AUDIO_INDEX]);
	ret = dequeue(&gv->head[AUDIO_INDEX], (uint8_t **)data, size);
	LeaveCriticalSection(&gv->cs[AUDIO_INDEX]);
	if (ret != 0)
		ret = -2;
	return ret;
}

//返回值 0=成功 其他=失败
//描述:停止采集
int stop_capture()
{
	int ret = 0, i = 0;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)gv->log, LOG_DEBUG, log_str);
	if (NULL == gv)
	{
		ret = -1;
		return ret;
	}

	gv->stop = 1;

	for (i=0; i<2; i++)
	{
		WaitForSingleObject(gv->handler[i],INFINITE);
	}

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)gv->log, LOG_DEBUG, log_str);

	ret = 0;
	return ret;
}

//释放采集所需要的资源
int free_capture()
{
	int ret = 0, i = 0;
	char log_str[1024] = {0};

	sprintf(log_str, ">>%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)gv->log, LOG_DEBUG, log_str);
	if (NULL == gv)
	{
		ret = -1;
		return ret;
	}

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
	sprintf(log_str, " %s:%d free gv\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)gv->log, LOG_DEBUG, log_str);

	sprintf(log_str, "<<%s:%d\r\n",__FUNCTION__, __LINE__);
	print_log((LOG *)gv->log, LOG_DEBUG, log_str);

	if (gv)
		free(gv);
	gv = NULL;
	ret = 0;
	return ret;
}
