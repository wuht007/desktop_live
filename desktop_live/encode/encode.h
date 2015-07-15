#ifndef __ENCODE_H__
#define __ENCODE_H__

#define BUILDING_DLL 1

#if BUILDING_DLL
# define DLLIMPORT __declspec (dllexport)
#else
# define DLLIMPORT __declspec (dllimport)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

DLLIMPORT int start_encode(void *log_file, char *config_file, char *record_file, int record);
DLLIMPORT int get_video_packet(void **data, unsigned long *size, long *pts, long *dts);
DLLIMPORT int get_audio_packet(void **data, unsigned long *size, long *pts, long *dts);
DLLIMPORT int stop_encode();

#ifdef __cplusplus
};
#endif

#endif //__ENCODE_H__