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

#ifdef __cplusplus
extern "C"
{
#endif

//参数日志指针、配置文件
//返回0=成功 其他失败
//开始采集
DLLIMPORT int start_capture(LOG *log, char *config_file);

//四个参数都是输出参数，保存数据的指针的地址、保存数据长度的地址、宽度的地址、高度的地址
//0=成功 其他=失败,函数执行成功需要释放data
//获取一帧video
DLLIMPORT int get_video_frame(void **data, unsigned long *size, int *width, int *hetgit);

//保存数据的指针的地址、保存数据长度的地址
//0=成功 其他=失败
//获取一帧audio
DLLIMPORT int get_audio_frame(void **data, unsigned long *size);

//停止采集
DLLIMPORT int stop_capture();

//释放采集所需要的资源
DLLIMPORT int free_capture();

#ifdef __cplusplus
};
#endif

#endif //__CAPTURE_H__