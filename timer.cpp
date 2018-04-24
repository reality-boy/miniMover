#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <Windows.h>
#include "timer.h"

LARGE_INTEGER msTime::StartingTime;
LARGE_INTEGER msTime::Frequency;
bool msTime::init = false;

float msTime::getTime_micro()
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

float msTime::getTime_s()
{ 
	return getTime_micro() / 1000000.0f;
}

float msTime::getTime_ms() 
{ 
	return getTime_micro() / 1000.0f;
}

msTimer::msTimer() 
{
	startTimer();
}

void msTimer::startTimer()
{ 
	startTime = msTime::getTime_micro(); 
	elapsedTime = 0;
}

float msTimer::stopTimer() 
{ 
	elapsedTime = msTime::getTime_micro() - startTime;
	return getLastTime_s();
}

float msTimer::getLastTime_s()
{ 
	return elapsedTime / 1000000.0f;
}

float msTimer::getLastTime_ms()
{ 
	return elapsedTime / 1000.0f; 
}

float msTimer::getLastTime_micro()
{ 
	return elapsedTime; 
}
