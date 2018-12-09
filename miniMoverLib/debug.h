#ifndef DEBUG_H
#define DEBUG_H

/*
Debug logging tools

The function debugPrint() logs the provided message to either the
console or the windows debug log if the indicated priority is report
or higher.

It also logs into the debug.txt file if found next to the exe
and the log level is one of log or higher.
*/

enum debugLevel
{
	DBG_WARN,
	DBG_REPORT,		// by default here and above logged to console
	DBG_LOG,		// by default here and above logged to disk
	DBG_VERBOSE,	// by default not logged
};

void debugInit();
void debugFinalize();

// set to true to drop debug log level when logging to the debug.txt file
// usefull when trying to supress a noisy message
void debugReduceNoise(bool doReduce);

// print the message to the debug console if priority is set right
void debugPrint(debugLevel l, const char *format, ...);
void debugPrintArray(debugLevel l, const char* data, int len);

#endif //DEBUG_H