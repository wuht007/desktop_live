#ifndef __CAPTURE_H__
#define __CAPTURE_H__

typedef struct fifo
{
	struct fifo *next;
	struct fifo *front;
	int size;
	void *data;
}FIFO;

/*
#ifdef __cplusplus
#define extern "C"
{
#endif
*/
extern int start_capture(FIFO *video_fifo_head, FIFO *audio_fifo_head, void *log_file);
extern int stop_capture();
/*
#ifdef __cplusplus
};
#endif
*/
#endif //__CAPTURE_H__