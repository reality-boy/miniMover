#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
# include <SDKDDKVer.h>
# include <Windows.h>
#else
# include <sys/time.h>
#endif

#include <stdio.h>

#include "timer.h"

long long msTime::StartingTime;
long long msTime::Frequency;
bool msTime::init = false;

float msTime::getTime_micro()
{
#ifdef _WIN32
	LARGE_INTEGER CurrentTime;
	QueryPerformanceCounter(&CurrentTime);

	if(!init)
	{
		StartingTime = CurrentTime.QuadPart;

		LARGE_INTEGER f;
		QueryPerformanceFrequency(&f);  // get conversion rate
		Frequency = f.QuadPart;

		init = true;
	}

	return (float) ((CurrentTime.QuadPart - StartingTime) * 1000000 / Frequency);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	long long CurrentTime = (1000000 * tv.tv_sec) + tv.tv_usec;

	if(!init)
	{
		StartingTime = CurrentTime;
		init = true;
	}

	return (float) (CurrentTime - StartingTime);

#endif
}

float msTime::getTime_s()
{ 
	return getTime_micro() / 1000000.0f;
}

float msTime::getTime_ms() 
{ 
	return getTime_micro() / 1000.0f;
}


//-------------------


msTimer::msTimer() 
	: m_minTime(-1.0f)
	, m_avgCount(0)
	, m_avgTime(0)
	, m_maxTime(-1.0f)

{
	startTimer();
}

void msTimer::startTimer()
{ 
	m_startTime = msTime::getTime_micro(); 
	m_elapsedTime = 0;
}

float msTimer::stopTimer() 
{ 
	if(m_startTime > -1)
	{
		m_elapsedTime = msTime::getTime_micro() - m_startTime;

		if(m_minTime < 0 || m_minTime > m_elapsedTime)
			m_minTime = m_elapsedTime;

		if(m_maxTime < m_elapsedTime)
			m_maxTime = m_elapsedTime;

		m_avgTime = ((m_avgTime * m_avgCount) + m_elapsedTime) / (m_avgCount + 1.0f);
		m_avgCount++;
	}
	m_startTime = -1;

	return getLastTime_s();
}

float msTimer::getLastTime_s() const
{ 
	return m_elapsedTime / 1000000.0f;
}

float msTimer::getLastTime_ms() const
{ 
	return m_elapsedTime / 1000.0f; 
}

float msTimer::getLastTime_micro() const
{ 
	return m_elapsedTime; 
}

float msTimer::getElapsedTime_s() const
{
	return (msTime::getTime_micro() - m_startTime) / 1000000.0f;
}

float msTimer::getMinTime_s() const
{
	return m_minTime / 1000000.0f;
}

float msTimer::getAvgTime_s() const
{
	return m_avgTime / 1000000.0f;
}

float msTimer::getMaxTime_s() const
{
	return m_maxTime / 1000000.0f;
}


//-------------------------


msTimeout::msTimeout()
{
	m_startTime = 0;
	m_endTime = 0;
}

msTimeout::msTimeout(float timeout_s)
{
	setTimeout_s(timeout_s);
}

void msTimeout::setTimeout_s(float timeout_s)
{
	m_startTime = msTime::getTime_s();
	m_endTime = m_startTime + timeout_s;
}

bool msTimeout::isTimeout() const
{
	return msTime::getTime_s() > m_endTime;
}

float msTimeout::getElapsedTime_s() const
{
	return msTime::getTime_s() - m_startTime;
}

float msTimeout::getElapsedTime_pct() const
{
	float elapsed = (msTime::getTime_s() - m_startTime) / (m_endTime - m_startTime);
	return (elapsed < 1.0f) ? elapsed : 1.0f;
}

