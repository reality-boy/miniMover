#ifndef TIMER_H
#define TIMER_H

#include <Windows.h>

class microsecondTimer
{
public:
	microsecondTimer() 
	{ 
		QueryPerformanceFrequency(&Frequency);  // get conversion rate
		QueryPerformanceCounter(&StartingTime); // start a timer
	}

	void startTimer() { QueryPerformanceCounter(&StartingTime); }

	float stopTimer()
	{
		LARGE_INTEGER EndingTime;
		QueryPerformanceCounter(&EndingTime);

		ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
		ElapsedMicroseconds.QuadPart *= 1000000;
		ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

		return ElapsedMicroseconds.QuadPart / 1000000.0f;
	}

	float getLastTime_s()     { return ElapsedMicroseconds.QuadPart / 1000000.0f; }
	float getLastTime_ms()    { return ElapsedMicroseconds.QuadPart /    1000.0f; }
	int getLastTime_micro() { return (int)ElapsedMicroseconds.QuadPart; }

protected:
	LARGE_INTEGER StartingTime;
	LARGE_INTEGER ElapsedMicroseconds;
	LARGE_INTEGER Frequency;
};

#endif // TIMER_H