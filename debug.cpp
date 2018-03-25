#include <SDKDDKVer.h>
#include <Windows.h>
#include <stdio.h>
#include <time.h>

#include "debug.h"

#pragma warning(disable:4996) // live on the edge!

FILE *g_debugLog = NULL;
debugLevel g_debugLevel = DBG_REPORT;

void debugInit()
{
	// protect against being called twice
	if(!g_debugLog)
	{
		// check if file exists
		g_debugLog = fopen("debug.txt", "r");

		// if found then append to file
		if(g_debugLog)
		{
			fclose(g_debugLog);
			g_debugLog = fopen("debug.txt", "a");
			if(g_debugLog)
			{
				time_t t = time(NULL);
				fprintf(g_debugLog, "\nLog started %s\n", ctime(&t));
			}
		}
	}
}

void debugFinalize()
{
	if(g_debugLog)
	{
		time_t t = time(NULL);
		fprintf(g_debugLog, "\nLog finished %s\n", ctime(&t));

		fclose(g_debugLog);
		g_debugLog = NULL;
	}
}

void debugPrint(debugLevel l, char *format, ...)
{
	const static int BUF_SIZE  = 2048;
	char msgBuf[BUF_SIZE];
	va_list arglist;

	va_start(arglist, format);
	_vsnprintf(msgBuf, sizeof(msgBuf), format, arglist);
	msgBuf[sizeof(msgBuf)-1] = '\0';
	va_end(arglist);

	// regular debug print
	if(l <= g_debugLevel)
	{
#ifdef _CONSOLE
		printf("%s\n", msgBuf);
#else
		OutputDebugString(msgBuf);
		OutputDebugString("\n");
#endif
	}

	// log to disk if error log is open
	// log at DBG_LOG level unless g_debugLevel is set to DBG_VERBOSE
	if(g_debugLog && l <= g_debugLevel || l <= DBG_LOG)
		fprintf(g_debugLog, "%s\n", msgBuf);
}
