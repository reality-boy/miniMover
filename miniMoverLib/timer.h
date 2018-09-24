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

	float getLastTime_s();
	float getLastTime_ms();
	float getLastTime_micro();

	float getElapsedTime_s();

protected:
	float m_startTime;
	float m_elapsedTime;
};

class msTimeout
{
public:
	msTimeout();
	msTimeout(float timeout_s);

	void setTimeout_s(float timeout_s);
	bool isTimeout();
	float getElapsedTime_s();
	float getElapsedTime_pct();

protected:
	float m_startTime;
	float m_endTime;
};

#endif // TIMER_H