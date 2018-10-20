#ifndef TIMER_H
#define TIMER_H

// get time using high resolution clock
class msTime
{
public:
	// get current time as a float
	static float getTime_s();		// seconds
	static float getTime_ms();		// milliseconds
	static float getTime_micro();	// microseconds

protected:
	static long long StartingTime;
	static long long Frequency; // only used in win32
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

	float getLastTime_s() const;
	float getLastTime_ms() const;
	float getLastTime_micro() const;

	float getElapsedTime_s() const;

	float getMinTime_s() const;
	float getAvgTime_s() const;
	float getMaxTime_s() const;

protected:
	float m_startTime;
	float m_elapsedTime;

	// timer stats
	float m_minTime;
	int m_avgCount;
	float m_avgTime;
	float m_maxTime;
};

class msTimeout
{
public:
	msTimeout();
	msTimeout(float timeout_s);

	void setTimeout_s(float timeout_s);
	bool isTimeout() const;
	float getElapsedTime_s() const;
	float getElapsedTime_pct() const;

protected:
	float m_startTime;
	float m_endTime;
};

#endif // TIMER_H