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

typedef struct 
{
	void *data;
	int size;
	long long pts;
	long long dts;
}AUDIO_PACKET;

DLLIMPORT int init_ercoder(void *log_file, char *config_file, char *record_file, int record);
DLLIMPORT int encode_video(void *source, int source_width, int source_height,
								void **dest, unsigned long *dest_size, long long *pts, long long *dts);
DLLIMPORT int encode_audio(void *source, unsigned long source_size, AUDIO_PACKET *ap, int *len);
DLLIMPORT int fflush_encoder();
DLLIMPORT int free_encoder();

#ifdef __cplusplus
};
#endif

#endif //__ENCODE_H__