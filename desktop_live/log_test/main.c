#include <Windows.h>
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdarg.h>
#include <stdio.h>
#include "log.h"

#pragma comment(lib, "log.lib")

int main()
{
	int ret = 0;
	//ret = init_log(LOG_DEBUG, OUT_FILE);
	ret = init_log(LOG_DEBUG, OUT_STDOUT);
	PRINT_LOG(LOG_DEBUG, "%s%d\n", "wuhongtao", 123);
	PRINT_LOG(LOG_DEBUG, "%s%d\n", "wuhongtao", 456);
	PRINT_LOG(LOG_DEBUG, "%s%d\n", "wuhongtao", 789);
	free_log();
	_CrtDumpMemoryLeaks();
	return 0;
}