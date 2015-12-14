#ifndef __LOG_H__
#define __LOG_H__

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

#define MYDEBUG 1
#if MYDEBUG
#define PRINT_LOG(level, format, ...) \
	print_log(level, format, __VA_ARGS__)
#else
#define PRINT_LOG(level, format, ...)
#endif

#define INITED		0
#define OPEN_FAILED -1
#define INIT_SECCESS 1
#define NOINIT		-2
#define LOW_LEVEL	-3


typedef enum level
{
	LOG_DEBUG	= 0,
	LOG_INFO	= 1,
	LOG_WARNING = 2,
	LOG_ERROR	= 3
}LEVEL;

typedef enum out_way
{
	OUT_FILE   = 0,
	OUT_STDOUT = 1
}OUT_WAY;

DLLIMPORT int init_log(LEVEL level, OUT_WAY out_way);

DLLIMPORT int print_log(LEVEL level, char *format, ...);

DLLIMPORT void free_log();
#ifdef __cplusplus
};
#endif

#endif //__LOG_H__