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

DLLIMPORT int init_ercoder(void *log_file, char *config_file, char *record_file, bool record);
DLLIMPORT int encode_video(void **data, unsigned long *size, long long pts, long long dts);
DLLIMPORT int encode_audio(void **data, unsigned long *size, long long pts, long long dts);
DLLIMPORT int free_encoder();

#ifdef __cplusplus
};
#endif

#endif //__ENCODE_H__