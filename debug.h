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
	DBG_ERR,
	DBG_WARN,
	DBG_REPORT,		
	DBG_LOG,
	DBG_VERBOSE,
};

void debugInit();
void debugFinalize();

// set to true to drop debug log level when logging to the debug.txt file
// usefull when trying to supress a noisy message
void debugReduceNoise(bool doReduce);

// print the message to the debug console if priority is set right
void debugPrint(debugLevel l, char *format, ...);

#endif //DEBUG_H