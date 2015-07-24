#ifndef __CAPTURE_H__
#define __CAPTURE_H__

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

DLLIMPORT int start_capture(void *log, char *config_file);
DLLIMPORT int get_video_frame(void **data, unsigned long *size, int *width, int *hetgit);
DLLIMPORT int get_audio_frame(void **data, unsigned long *size);
DLLIMPORT int stop_capture();
DLLIMPORT int free_capture();

#ifdef __cplusplus
};
#endif

#endif //__CAPTURE_H__