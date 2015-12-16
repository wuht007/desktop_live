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
	ret = InitLog(LOG_DEBUG, OUT_FILE);
	//ret = InitLog(LOG_DEBUG, OUT_STDOUT);
	PRINTLOG(LOG_DEBUG, "%s%d\n", "wuhongtao", 123, 999);
	PRINTLOG(LOG_DEBUG, "%s%d\n", "wuhongtao");
	PRINTLOG(LOG_DEBUG, "%s%d\n", "wuhongtao", 789);
	PRINTLOG(LOG_DEBUG, "%s %d %s\n", __FILE__, __LINE__, __FUNCTION__);
	FreeLog();
	_CrtDumpMemoryLeaks();
	return 0;
}