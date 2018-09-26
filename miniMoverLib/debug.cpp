#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
# include <SDKDDKVer.h>
# include <Windows.h>
# pragma warning(disable:4996) // live on the edge!
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "timer.h"
#include "debug.h"

FILE *g_debugLog = NULL;
//int g_callCount = 0;

// console log level
debugLevel g_debugLevel = DBG_REPORT;
// debug.txt log level
debugLevel g_debugDiskLevel = DBG_LOG;
bool g_doReduceNoise = false;

msTimer g_time;

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
				g_time.startTimer();
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
		fprintf(g_debugLog, "\n\nLog finished %s\n", ctime(&t));
		//fprintf(g_debugLog, "Call count %d\n", g_callCount);

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
	//g_callCount++;

	const static int BUF_SIZE  = 2048;
	static char lastPrintMsgBuf[BUF_SIZE] = "";
	static char lastDiskMsgBuf[BUF_SIZE] = "";
	char msgBuf[BUF_SIZE];
	va_list arglist;

	va_start(arglist, format);
	vsnprintf(msgBuf, sizeof(msgBuf), format, arglist);
	msgBuf[sizeof(msgBuf)-1] = '\0';
	va_end(arglist);

	// regular debug print
	if(l <= g_debugLevel)
	{
		if(0 != strcmp(lastPrintMsgBuf, msgBuf))
		{
#if defined(_WIN32) && !defined(_CONSOLE)
			OutputDebugString("\n");
			OutputDebugString(msgBuf);
#else
			printf("\n%s", msgBuf);
#endif
			strcpy(lastPrintMsgBuf, msgBuf);
		}
		else
		{
#if defined(_WIN32) && !defined(_CONSOLE)
			OutputDebugString(".");
#else
			printf(".");
#endif
		}
	}

	// log to disk if error log is open
	if(g_debugLog != NULL && (l <= g_debugLevel || (!g_doReduceNoise && l <= g_debugDiskLevel)))
	{
		if(0 != strcmp(lastDiskMsgBuf, msgBuf))
		{
			fprintf(g_debugLog, "\n%07.2f %s", g_time.getElapsedTime_s(), msgBuf);
			fflush(g_debugLog);
			strcpy(lastDiskMsgBuf, msgBuf);
		}
		else
		{
			fprintf(g_debugLog, ".");
		}
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
			OutputDebugString("\n");
			for(int i=0; i<len; i++)
			{
				sprintf(tstr, " %02x", data[i]);
				OutputDebugString(tstr);
			}
#else
			printf("\n");
			for(int i=0; i<len; i++)
			{
				printf(" %02x", data[i]);
			}
#endif
		}

		// log to disk if error log is open
		if(g_debugLog != NULL && (l <= g_debugLevel || (!g_doReduceNoise && l <= g_debugDiskLevel)))
		{
			fprintf(g_debugLog, "\n");
			fprintf(g_debugLog, "%07.2f", g_time.getElapsedTime_s());
			for(int i=0; i<len; i++)
			{
				fprintf(g_debugLog, " %02x", data[i]);
			}
			fflush(g_debugLog);
		}
	}
}
