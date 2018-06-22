#ifndef STREAM_H
#define STREAM_H

/*
Base class for serial based communicaiton protocols.
*/

class Stream
{
public:
	Stream() 
		: m_lineBufCount(0)
	{}

	virtual ~Stream() {}

	// functions handled by derived class

	// close connection
	virtual void closeStream() = 0;

	// are we currently connected
	virtual bool isOpen() = 0;

	// empty input buffer
	virtual void clear() = 0;

	// read all data available
	virtual int read(char *buf, int bufLen) = 0;

	// write a fixed length buffer
	virtual int write(const char *buf, int bufLen) = 0;

	// local functions, don't override

	// read only to newline char, buffering rest of data, return immediately if not found
	int readLine(char *buf, int bufLen);
	// block for timeout seconds before returning
	int readLineWait(char *buf, int bufLen, float timeout_s = -1, bool report = true);

	// write a null terminated string
	int writeStr(const char *buf);
	// write a string formatted by printf
	int writePrintf(const char *fmt, ...);

private:

	// default communication timeout
	float getDefaultTimeout() { return 5.0f; }

	// helper function to read a line from m_lineBuf
	int readLineFromBuffer(char *buf, int bufLen);

	// readLine data
	const static int m_lineBufLen = 4096;
	int m_lineBufCount;
	char m_lineBuf[m_lineBufLen];
};

#endif // STREAM_H