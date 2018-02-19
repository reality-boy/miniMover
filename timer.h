#ifndef TIMER_H
#define TIMER_H

#include <Windows.h>

class msTime
{
public:
	static float getTime_micro()
	{
		if(!init)
		{
			QueryPerformanceFrequency(&Frequency);  // get conversion rate
			QueryPerformanceCounter(&StartingTime); // start a timer
			init = true;
		}

		LARGE_INTEGER CurrentTime;
		QueryPerformanceCounter(&CurrentTime);

		return (float) ((CurrentTime.QuadPart - StartingTime.QuadPart) * 1000000 / Frequency.QuadPart);
	}

	static float getTime_s() { return getTime_micro() / 1000000.0f; }
	static float getTime_ms() { return getTime_micro() / 1000.0f; }

protected:
	static LARGE_INTEGER StartingTime;
	static LARGE_INTEGER Frequency;
	static bool init;
};

class msTimer
{
public:
	msTimer() 
		: startTime(0)
		, elapsedTime(0)
	{}

	void startTimer() { startTime = msTime::getTime_micro(); }
	float stopTimer() 
	{ 
		elapsedTime = msTime::getTime_micro() - startTime;
		return elapsedTime;
	}

	float getLastTime_s()     { return elapsedTime / 1000000.0f; }
	float getLastTime_ms()    { return elapsedTime /    1000.0f; }
	float getLastTime_micro() { return elapsedTime; }

protected:
	float startTime;
	float elapsedTime;
};

#endif // TIMER_H