#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <Windows.h>
#include "capture.h"
#include "list.h"

#ifndef MIN
#  define MIN(a,b)  ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#  define MAX(a,b)  ((a) < (b) ? (b) : (a))
#endif

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "log.lib")

//描述:获取屏幕的宽高，屏幕位图的宽高、深度、通道数、一帧位图的长度
void GetScreenInfo(PSCREEN pScreen)
{
	HDC src = NULL;
	HDC mem = NULL;
	HBITMAP bitmap = NULL;
	HBITMAP old_bitmap = NULL;
	BITMAP bmp;

	int left = 0;
	int top = 0;
	int right = GetSystemMetrics(SM_CXSCREEN);
	int bottom = GetSystemMetrics(SM_CYSCREEN);

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	pScreen->width = right - left;
	pScreen->height = bottom - top;

	src = CreateDC("DISPLAY", NULL, NULL, NULL);
	mem = CreateCompatibleDC(src);

	bitmap = CreateCompatibleBitmap(src, pScreen->width, pScreen->height);
	old_bitmap = (HBITMAP)SelectObject(mem, bitmap);

	BitBlt(mem, 0, 0, pScreen->width, pScreen->height, src, 0, 0, SRCCOPY);
	bitmap = (HBITMAP)SelectObject(mem, old_bitmap);
	GetObject(bitmap, sizeof(BITMAP), &bmp);

	pScreen->bitmap_channel = bmp.bmBitsPixel == 1 ? 1 : bmp.bmBitsPixel/8 ;
	pScreen->bitmap_depth = bmp.bmBitsPixel == 1 ? 1 : 8;//IPL_DEPTH_1U : IPL_DEPTH_8U;
	pScreen->bitmap_width = bmp.bmWidth;
	pScreen->bitmap_height = bmp.bmHeight;
	pScreen->len = pScreen->bitmap_channel * (pScreen->bitmap_depth/8) * \
					pScreen->bitmap_width * pScreen->bitmap_height;

	PRINTLOG(LOG_INFO, "screen->width=%d screen->height=%d screen->bitmap_width=%d screen->bitmap_height=%d screen->len=%d\n",
					   pScreen->width, pScreen->height, pScreen->bitmap_width, pScreen->bitmap_height, pScreen->len);

	SelectObject(mem,old_bitmap);
	DeleteObject(old_bitmap);
	DeleteDC(mem);
	SelectObject(src,bitmap);
	DeleteDC(mem);
	DeleteObject(bitmap);

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
}

int InitCaptureVideoParam(PSCREEN pScreen, PVIDEO pVideo, PCAPTURECONFIG pCaptureConfig)
{
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	pVideo->fps = pCaptureConfig->fps;
	PRINTLOG(LOG_INFO,"pVideo->fps = %d\n", pVideo->fps);

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return INIT_SECCESS;
}

//描述:把桌面设备的内存位图复制到video->rgba
void CopyScreenBitmap(PSCREEN pScreen, PVIDEO pVideo)
{
	HDC src = NULL;
	HDC mem = NULL;
	HBITMAP bitmap = NULL;
	HBITMAP old_bitmap = NULL;

	src = CreateDC("DISPLAY", NULL, NULL, NULL);
	mem = CreateCompatibleDC(src);
	bitmap = CreateCompatibleBitmap(src, pScreen->width, pScreen->height);
	old_bitmap = (HBITMAP)SelectObject(mem, bitmap);

	BitBlt(mem, 0, 0, pScreen->width, pScreen->height, src, 0, 0 , SRCCOPY);
	bitmap = (HBITMAP)SelectObject(mem, old_bitmap);
	GetBitmapBits(bitmap, pScreen->len, pVideo->rgba);
	
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
	*len = nWidth * nHeight + (nWidth*nHeight)/2;
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

int MallocVideobuffer(PSCREEN pScreen, PVIDEO pVideo)
{
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	pVideo->yuv_len = pScreen->bitmap_width * \
		pScreen->bitmap_height*2;
	pVideo->yuv = (uint8_t *)malloc(pVideo->yuv_len);
	if (!pVideo->yuv)
	{
		return MALLOCFAILED;
	}

	PRINTLOG(LOG_DEBUG, "pVideo->yuv malloc %d\n", pVideo->yuv_len);

	pVideo->rgba_len =  pScreen->bitmap_width * pScreen->bitmap_height * \
		pScreen->bitmap_channel * (pScreen->bitmap_depth/8);
	pVideo->rgba = (uint8_t *)malloc(pVideo->rgba_len);
	if (!pVideo->rgba)
	{
		return MALLOCFAILED;
	}

	PRINTLOG(LOG_DEBUG, "pVideo->rgba malloc %d\n", pVideo->rgba_len);
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	return SECCESS;
}

void WINAPI onTimeFunc(UINT wTimerID, UINT msg,DWORD dwUser,DWORD dwl,DWORD dw2)
{
	PCAPTURE pCapture = (PCAPTURE)dwUser;
	if (1 != pCapture->stop)
	{
		CopyScreenBitmap(&pCapture->screen ,&pCapture->video);
		RGBA2YUV((LPBYTE)pCapture->video.rgba, 
			pCapture->screen.bitmap_width, 
			pCapture->screen.bitmap_height, 
			(LPBYTE)pCapture->video.yuv, 
			&pCapture->video.yuv_len, 
			pCapture->screen.bitmap_width*4);

		EnterCriticalSection(&pCapture->cs[VIDEO_INDEX]);
		enqueue(&pCapture->head[VIDEO_INDEX], pCapture->video.yuv, pCapture->video.yuv_len);
		LeaveCriticalSection(&pCapture->cs[VIDEO_INDEX]);
	}

}

//描述:采集视频的主逻辑，包括屏幕信息获取，配置文件参数获取，采集转码和入队
unsigned int __stdcall VideoCaptureProc(void *p)
{
	unsigned int step_time = 0;
	DWORD start = 0;
	DWORD end = 0;
	MMRESULT timer_id = 0;
	PCAPTURE pCapture = (PCAPTURE)p;

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (SECCESS != 
		MallocVideobuffer(&pCapture->screen, &pCapture->video))
	{
		PRINTLOG(LOG_ERROR, "%s %d MallocVideobuffer malloc failed\n", __FUNCTION__, __LINE__);
		return MALLOCFAILED;
	}

	step_time = 1000 / pCapture->video.fps;
	PRINTLOG(LOG_INFO, "capture step_time=%d\n", step_time);

	timer_id = timeSetEvent(step_time, 10, onTimeFunc, (DWORD_PTR)pCapture, TIME_PERIODIC);

/*	while (0 == s_capture.stop)
	{
		Sleep(100);
		start = timeGetTime();
		CopyScreenBitmap(&s_capture.screen ,&s_capture.video);
		RGBA2YUV((LPBYTE)s_capture.video.rgba, 
					s_capture.screen.bitmap_width, 
					s_capture.screen.bitmap_height, 
					(LPBYTE)s_capture.video.yuv, 
					&s_capture.video.yuv_len, 
					s_capture.screen.bitmap_width*4);

		EnterCriticalSection(&s_capture.cs[VIDEO_INDEX]);
		enqueue(&s_capture.head[VIDEO_INDEX], s_capture.video.yuv, s_capture.video.yuv_len);
		LeaveCriticalSection(&s_capture.cs[VIDEO_INDEX]);
		while((((end = timeGetTime()) - start) < step_time))
			;
	}
*/

	WaitForSingleObject(pCapture->hEvent[VIDEO_INDEX], INFINITE);
	timeKillEvent(timer_id);

	free(pCapture->video.yuv);
	PRINTLOG(LOG_DEBUG, "free video.yuv\n");

	free(pCapture->video.rgba);
	PRINTLOG(LOG_DEBUG, "free video.rgba\n");


	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return 0;
}

void InitCaptureAudioParam(PAUDIO pAudio, PCAPTURECONFIG pCaptureConfig)
{
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	pAudio->channels = pCaptureConfig->channels;
	pAudio->bits_per_sample = pCaptureConfig->bits_per_sample;
	pAudio->samples_per_sec = pCaptureConfig->samples_per_sec;
	pAudio->avg_bytes_per_sec = pCaptureConfig->avg_bytes_per_sec;
	PRINTLOG(LOG_INFO, "pAudio->channels=%d pAudio->bits_per_sample=%d pAudio->samples_per_sec=%d pAudio->avg_bytes_per_sec=%d\n",
		pAudio->channels, pAudio->bits_per_sample, pAudio->samples_per_sec, pAudio->avg_bytes_per_sec);

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
}

//描述:初始化并且开启wave采集
int StartWave(PAUDIO pAudio, PWAVEFORMATEX pWaveFormat, 
				PWAVEHDR *wavehdr, HWAVEIN *wavein, const int HDRCOUNT)
{
	int ret = 0;
	int i = 0;
	int size = 1024*24;

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	PRINTLOG(LOG_INFO, "wavein header buffer size=%d header count=%d\n",size, HDRCOUNT);

	pAudio->pcm_len = 0;
	pAudio->pcm = (uint8_t *)malloc(size * HDRCOUNT);
	if (NULL == pAudio->pcm)
	{
		return MALLOCFAILED;
	}
	PRINTLOG(LOG_DEBUG, "audio->pcm = malloc %d\n", size*HDRCOUNT);

	pWaveFormat->wFormatTag = WAVE_FORMAT_PCM;								//采样格式
	pWaveFormat->nChannels = pAudio->channels;								//声道数
	pWaveFormat->nSamplesPerSec = pAudio->samples_per_sec;					//采样率
	pWaveFormat->nAvgBytesPerSec = pAudio->avg_bytes_per_sec;					//平均码率
	pWaveFormat->nBlockAlign = pAudio->channels * pAudio->bits_per_sample / 8;	//声道数*每个样本占的字节数
	pWaveFormat->wBitsPerSample = pAudio->bits_per_sample;					//样本占的位数
	pWaveFormat->cbSize = 0;													//特别信息计数

	ret = waveInOpen(wavein, WAVE_MAPPER, pWaveFormat, (DWORD)NULL, 0L, CALLBACK_NULL);
	if (ret != MMSYSERR_NOERROR)
	{
		return WAVEINOPENFAILED;
	}

	for (i=0; i<HDRCOUNT; i++)
	{
		wavehdr[i] = (WAVEHDR*)malloc(size + sizeof(WAVEHDR));
		if (NULL == wavehdr[i])
		{
			return WAVEINOPENFAILED;
		}
		PRINTLOG(LOG_DEBUG, "wavehdr[%d] = malloc %d\n", i, size*HDRCOUNT);

		memset(wavehdr[i], 0, size + sizeof(WAVEHDR));
		wavehdr[i]->dwBufferLength = size;
		wavehdr[i]->lpData = (LPSTR)(wavehdr[i] + 1);//(LPSTR)((char *)wavehdr[i]+sizeof(WAVEHDR));

		ret = waveInPrepareHeader(*wavein, wavehdr[i], sizeof(WAVEHDR));
		if (ret != MMSYSERR_NOERROR)
		{
			return WAVEINPREPAREHEADERFAILED;
		}

		ret = waveInAddBuffer(*wavein, wavehdr[i], sizeof(WAVEHDR));
		if (ret != MMSYSERR_NOERROR)
		{
			return WAVEINADDBUFFERFAILED;
		}
	}

	ret = waveInStart(*wavein);
	if (ret != MMSYSERR_NOERROR)
	{
		return WAVEINSTARTFAILED;
	}

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return SECCESS;
}

//描述:音频采集主逻辑
unsigned int __stdcall AudioCaptureProc(void *p)
{
	int i = 0;
	int ret = 0;
	const int HDRCOUNT = 10;
	int hdr = 0;
	WAVEHDR *wavehdr[10];
	WAVEFORMATEX waveformat = {0};
	HWAVEIN wavein = {0};
	PCAPTURE pCapture = (PCAPTURE)p;

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	ret = StartWave(&pCapture->audio, &waveformat, &wavehdr[0], &wavein, HDRCOUNT);
	if (SECCESS != ret)
	{
		PRINTLOG(LOG_ERROR, ">>%s %s %d StartWave failed return code=%d\n",
			__FILE__, __FUNCTION__, __LINE__, ret);
		return ret;
	}


	while (0 == pCapture->stop)
	{
		Sleep(100);
		while (wavehdr[hdr]->dwFlags & WHDR_DONE)
		{
			memcpy(pCapture->audio.pcm + pCapture->audio.pcm_len,
				wavehdr[hdr]->lpData, wavehdr[hdr]->dwBytesRecorded);
			pCapture->audio.pcm_len += wavehdr[hdr]->dwBytesRecorded;
			ret = waveInAddBuffer(wavein, wavehdr[hdr], sizeof(WAVEHDR));            
			if (ret != MMSYSERR_NOERROR)
			{
				PRINTLOG(LOG_ERROR, ">>%s %s %d waveInAddBuffer failed ret = %d\n",
					__FILE__, __FUNCTION__, __LINE__, ret);
				return WAVEINADDBUFFERFAILED;
			}
			hdr = (hdr+1)%HDRCOUNT;
		}

		EnterCriticalSection(&pCapture->cs[AUDIO_INDEX]);
		enqueue(&pCapture->head[AUDIO_INDEX], pCapture->audio.pcm, pCapture->audio.pcm_len);
		pCapture->audio.pcm_len = 0;
		LeaveCriticalSection(&pCapture->cs[AUDIO_INDEX]);
	}

	//WaitForSingleObject(pCapture->hEvent[AUDIO_INDEX], INFINITE);

	ret = waveInStop(wavein);
	if (ret != MMSYSERR_NOERROR) 
	{
		return WAVEINSTOPFAILED;
	}

	ret = waveInReset(wavein);
	if (ret != MMSYSERR_NOERROR)
	{
		return WAVEINRESETFAILED;
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
		PRINTLOG(LOG_DEBUG, "wavehdr[%d] free\n", i);
	}

	free(pCapture->audio.pcm);
	PRINTLOG(LOG_DEBUG, "audio.pcm free\n");

	ret = waveInClose(wavein);
	if (ret != MMSYSERR_NOERROR)
	{
		return WAVEINCLOSEFAILED;
	}
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return SECCESS;
}

PCAPTURE InitCapture(PCAPTURECONFIG pCaptureConfig)
{
	int ret;
	PCAPTURE pCapture = NULL;
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (NULL == pCaptureConfig ||
		0 >= pCaptureConfig->fps ||
		0 >= pCaptureConfig->channels ||
		0 >= pCaptureConfig->bits_per_sample ||
		0 >= pCaptureConfig->samples_per_sec ||
		0 >= pCaptureConfig->avg_bytes_per_sec)
	{
		PRINTLOG(LOG_ERROR, "%s %d pCaptureConfig param error\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	pCapture = (PCAPTURE)malloc(sizeof(CAPTURE));
	if (NULL == pCapture)
	{
		PRINTLOG(LOG_ERROR, "%s %d pCapture malloc failed\n", __FUNCTION__, __LINE__);
		return NULL;
	}
	PRINTLOG(LOG_DEBUG,"pCapture = malloc %d\n", sizeof(CAPTURE));

	memset(pCapture, 0, sizeof(CAPTURE));

	GetScreenInfo(&pCapture->screen);
	ret = InitCaptureVideoParam(&pCapture->screen, &pCapture->video, pCaptureConfig);
	if (ret != INIT_SECCESS)
	{
		PRINTLOG(LOG_ERROR, "%s %d InitCaptureVideoParam failed return code:%d\n", __FUNCTION__, __LINE__, ret);
		free(pCapture);
		return NULL;
	}

	InitCaptureAudioParam(&pCapture->audio, pCaptureConfig);

	pCapture->initialized = 1;
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return pCapture;
}

int StartCapture(PCAPTURE pCapture)
{
	int ret = 0;
	int i = 0;

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (pCapture->initialized != 1)
	{
		PRINTLOG(LOG_ERROR, "%s %d capture have no init\n", __FUNCTION__, __LINE__);
		return NOINIT;
	}

	pCapture->stop = 0;


	for (i=0; i<ARRAY_LEN; i++)
	{
		INIT_LIST_HEAD(&pCapture->head[i]);
		InitializeCriticalSection(&pCapture->cs[i]);
		pCapture->hEvent[i] = CreateEvent(NULL,FALSE,FALSE,NULL);
		if (NULL == pCapture->hEvent[i])
		{
			PRINTLOG(LOG_ERROR, "create event failed\n");
			return CREATE_EVENT_FAILED;
		}

		if (VIDEO_INDEX == i)
			pCapture->handler[i] = (HANDLE)_beginthreadex(NULL, 0, VideoCaptureProc, pCapture, 0, NULL);
		else if (AUDIO_INDEX == i)
			pCapture->handler[i] = (HANDLE)_beginthreadex(NULL, 0, AudioCaptureProc, pCapture, 0, NULL);

		if (NULL == pCapture->handler[i])
		{
			PRINTLOG(LOG_ERROR, "start thread failed\n");
			return STARTTHREADFAILED;
		}
	}

	pCapture->started = 1;
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	return SECCESS;
}

//描述:获取一帧采集视频数据
int GetVideoFrame(PCAPTURE pCapture, void **data, unsigned long *size, int *width, int *hetgit)
{
	int ret = 0;
	if (NULL == data || NULL == size)
	{
		return WRONG_PARAM;
	}

	if (1 != pCapture->initialized)
	{
		return NOINIT;
	}

	if (1 != pCapture->started)
	{
		return NOSTART;
	}

	EnterCriticalSection(&pCapture->cs[VIDEO_INDEX]);
	ret = dequeue(&pCapture->head[VIDEO_INDEX], (uint8_t **)data, size);
	LeaveCriticalSection(&pCapture->cs[VIDEO_INDEX]);
	if (ret != 0)
		return DEQUEUEFAILED;
	*width = pCapture->screen.width;
	*hetgit = pCapture->screen.height;

	return SECCESS;
}

//描述:获取一帧采集音频数据
int GetAudioFrame(PCAPTURE pCapture, void **data, unsigned long *size)
{
	int ret = 0;
	if (NULL == data || NULL == size )
	{
		return WRONG_PARAM;
	}

	if (1 != pCapture->initialized)
	{
		return NOINIT;
	}

	if (1 != pCapture->started)
	{
		return NOSTART;
	}

	EnterCriticalSection(&pCapture->cs[AUDIO_INDEX]);
	ret = dequeue(&pCapture->head[AUDIO_INDEX], (uint8_t **)data, size);
	LeaveCriticalSection(&pCapture->cs[AUDIO_INDEX]);
	if (ret != 0)
		return DEQUEUEFAILED;
	return SECCESS;
}

//描述:停止采集
int StopCapture(PCAPTURE pCapture)
{
	int i = 0;

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (pCapture->initialized != 1)
	{
		PRINTLOG(LOG_ERROR, "capture have no init\n");
		return NOINIT;
	}

	if (1 != pCapture->started)
	{
		PRINTLOG(LOG_ERROR, "capture have no start\n");
		return NOSTART;
	}

	pCapture->stop = 1;

	for (i=0; i<ARRAY_LEN; i++)
	{
		SetEvent(pCapture->hEvent[i]);
		WaitForSingleObject(pCapture->handler[i],INFINITE);
		CloseHandle(pCapture->hEvent[i]);
		pCapture->handler[i] = NULL;
	}

	pCapture->started = 0;
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	return SECCESS;
}

//释放采集所需要的资源
int FreeCapture(PCAPTURE pCapture)
{
	int ret = 0, i = 0;
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (pCapture->initialized != 1)
	{
		PRINTLOG(LOG_ERROR, "capture have no init\n");
		return NOINIT;
	}


	for (i=0; i<ARRAY_LEN; i++)
	{
		struct list_head *plist = NULL, *n = NULL;
		if (NULL != pCapture->handler[i])
		{
			return NOSTOP;
		}

		EnterCriticalSection(&pCapture->cs[i]);

		list_for_each_safe(plist, n, &pCapture->head[i]) 
		{
			NODE *node = list_entry(plist, struct node, list);
			list_del(plist);
			free(node->data);
			free(node);
		}

		LeaveCriticalSection(&pCapture->cs[i]);
		DeleteCriticalSection(&pCapture->cs[i]);
	}

	pCapture->initialized = 0;
	free(pCapture);
	PRINTLOG(LOG_DEBUG, "pCapture free\n");
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	return SECCESS;
}
