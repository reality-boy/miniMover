#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
# include <SDKDDKVer.h>
# include <Windows.h>
# pragma warning(disable:4996) // live on the edge!
#endif

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "debug.h"

FILE *g_debugLog = NULL;

// console log level
debugLevel g_debugLevel = DBG_REPORT;
// debug.txt log level
debugLevel g_debugDiskLevel = DBG_LOG;
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
				fflush(g_debugLog);
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
	vsnprintf(msgBuf, sizeof(msgBuf), format, arglist);
	msgBuf[sizeof(msgBuf)-1] = '\0';
	va_end(arglist);

	// regular debug print
	if(l <= g_debugLevel)
	{
#if defined(_WIN32) && !defined(_CONSOLE)
		OutputDebugString(msgBuf);
		OutputDebugString("\n");
#else
		printf("%s\n", msgBuf);
#endif
	}

	// log to disk if error log is open
	if(g_debugLog != NULL && (l <= g_debugLevel || (!g_doReduceNoise && l <= g_debugDiskLevel)))
	{
		fprintf(g_debugLog, "%s\n", msgBuf);
		fflush(g_debugLog);
	}
}

void debugPrintArray(debugLevel l, const char* data, int len)
{
	if(data && len > 0)
	{
		// regular debug print
		if(l <= g_debugLevel)
		{
#if defined(_WIN32) && !defined(_CONSOLE)
			char tstr[25];
			for(int i=0; i<len; i++)
			{
				sprintf(tstr, " %02x", data[i]);
				OutputDebugString(tstr);
			}
			OutputDebugString("\n");
#else
			for(int i=0; i<len; i++)
			{
				printf(" %02x", data[i]);
			}
			printf("\n");
#endif
		}

		// log to disk if error log is open
		if(g_debugLog != NULL && (l <= g_debugLevel || (!g_doReduceNoise && l <= g_debugDiskLevel)))
		{
			for(int i=0; i<len; i++)
			{
				fprintf(g_debugLog, " %02x", data[i]);
			}
			fprintf(g_debugLog, "\n");
			fflush(g_debugLog);
		}
	}
}
