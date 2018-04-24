#ifndef TIMER_H
#define TIMER_H

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <Windows.h>

// get time using high resolution clock
class msTime
{
public:
	// get current time as a float
	static float getTime_s();		// seconds
	static float getTime_ms();		// milliseconds
	static float getTime_micro();	// microseconds

protected:
	static LARGE_INTEGER StartingTime;
	static LARGE_INTEGER Frequency;
	static bool init;
};

// track elapsed time at the millisecond level
// more of a stop watch than a timer...
class msTimer
{
public:
	msTimer();

	void startTimer();
	float stopTimer();

	float getLastTime_s();
	float getLastTime_ms();
	float getLastTime_micro();

protected:
	float startTime;
	float elapsedTime;
};

#endif // TIMER_H