#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

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

#define LOG_DEBUG		0
#define	LOG_INFO		1
#define	LOG_WARNING		2
#define	LOG_ERROR		3

enum LEVEL
{
	ENUM_DEBUG	= 0,
	ENUM_INFO	= 1,
	ENUM_WARNING= 2,
	ENUM_ERROR	= 3
};

enum OUT_WAY
{
	OUT_FILE=0,
	OUT_STDOUT=1
};

typedef struct logpriv
{
	enum LEVEL log_level;
	enum OUT_WAY out_way;
	FILE *file;
	RTL_CRITICAL_SECTION cs;
}LOG_PRIV;

typedef struct
{
	struct logpriv log_priv;
}LOG;

DLLIMPORT LOG *init_log(char *file_name, unsigned int level, unsigned int out_way);
DLLIMPORT int print_log(LOG *log, unsigned int log_level, char *log_str);
DLLIMPORT void free_log();

#ifdef __cplusplus
};
#endif

#endif //__LOG_H__