#ifndef DEBUG_H
#define DEBUG_H

//****FixMe, 
// look for debug.txt file at startup and if found then bump debug priority up to high and log to file
// implement debug priority 
// log all serial reads/writes other than printing, only log the transitions and not the raw (megabytes worth of) data
// possibly avoid logging of status as well, look into it.

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
void debugReduceNoise(bool doReduce);
void debugPrint(debugLevel l, char *format, ...);

#endif //DEBUG_H