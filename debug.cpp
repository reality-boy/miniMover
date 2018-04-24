#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <Windows.h>
#include <stdio.h>
#include <time.h>

#include "debug.h"

#pragma warning(disable:4996) // live on the edge!

FILE *g_debugLog = NULL;
debugLevel g_debugLevel = DBG_REPORT;
bool g_doReduceNoise = false;

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

void debugReduceNoise(bool doReduce)
{
	g_doReduceNoise = doReduce;
}

void debugPrint(debugLevel l, const char *format, ...)
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
	if(g_debugLog != NULL && (l <= g_debugLevel || (!g_doReduceNoise && l <= DBG_LOG)))
		fprintf(g_debugLog, "%s\n", msgBuf);
}

void debugPrintArray(debugLevel l, const char* data, int len)
{
	if(data && len > 0)
	{
		// regular debug print
		if(l <= g_debugLevel)
		{
#ifdef _CONSOLE
			for(int i=0; i<len; i++)
				printf(" %02x", data[i]);
			printf("\n");
#else
			char tstr[25];
			for(int i=0; i<len; i++)
			{
				sprintf(tstr, " %02x", data[i]);
				OutputDebugString(tstr);
			}
			OutputDebugString("\n");
#endif
		}

		// log to disk if error log is open
		// log at DBG_LOG level unless g_debugLevel is set to DBG_VERBOSE
		if(g_debugLog != NULL && (l <= g_debugLevel || (!g_doReduceNoise && l <= DBG_LOG)))
		{
			for(int i=0; i<len; i++)
				fprintf(g_debugLog, " %02x", data[i]);
			fprintf(g_debugLog, "\n");
		}
	}
}
