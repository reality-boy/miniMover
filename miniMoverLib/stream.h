#ifndef STREAM_H
#define STREAM_H

/*
Base class for serial based communicaiton protocols.
*/

#include "timer.h"

class StreamT
{
public:
	StreamT() 
		: m_lineBufCount(0)
	{
		*m_lineBuf = '\0';
	}

	virtual ~StreamT() {}

	// functions handled by derived class

	// bool openStream(whatever parameters define the stream)

	// reopen the stream, using the last parameters
	// needed for wifi connections, since the 1.1 Plus machine closes connection
	// after every single message is sent!
	virtual bool reopenStream() = 0;

	// close connection
	virtual void closeStream() = 0;

	// are we currently connected
	virtual bool isOpen() = 0;

	// empty input buffer
	virtual void clear() = 0;

	// read all data available
	virtual int read(char *buf, int bufLen, bool zeroTerminate = true) = 0;

	// write a fixed length buffer
	virtual int write(const char *buf, int bufLen) = 0;

	virtual bool isWIFI() = 0;

	// default communication timeout
	virtual float getDefaultTimeout() = 0;

	// local functions, don't override

	// read only to newline char, buffering rest of data, return immediately if not found
	int readLine(char *buf, int bufLen, bool readPartial = false);

	// write a null terminated string
	int writeStr(const char *buf);

	// write a string formatted by printf
	int writePrintf(const char *fmt, ...);

	static bool isNetworkAddress(const char *addr);

	msTimer m_msgTimer;

//protected:

	// helper function to read a line from m_lineBuf
	int readLineFromBuffer(char *buf, int bufLen, bool readPartial);
	bool isLineInBuffer();

	// readLine data
	const static int m_lineBufLen = 4096;
	int m_lineBufCount;
	char m_lineBuf[m_lineBufLen];
};

#endif // STREAM_H