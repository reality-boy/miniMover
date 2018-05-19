#ifndef STREAM_H
#define STREAM_H

/*
Base class for serial based communicaiton protocols.
*/

class Stream
{
public:
	Stream() 
		: m_lineBufStart(m_lineBuf)
		, m_lineBufEnd(m_lineBuf)
	{}

	~Stream() {}

	// are we currently connected
	virtual bool isOpen() = 0;

	// empty input buffer
	virtual void clear() = 0;

	// read all data available
	virtual int read(char *buf, int bufLen) = 0;
	// read only to newline char, buffering rest of data
	virtual int readLine(char *buf, int bufLen);

	// write a fixed length buffer
	virtual int write(const char *buf, int bufLen) = 0;
	// write a null terminated string
	virtual int writeStr(const char *buf);
	// write a string formatted by printf
	virtual int writePrintf(const char *fmt, ...);

protected:

	// readLine data
	const static int m_lineBufLen = 4096;
	char m_lineBuf[m_lineBufLen];
	char* m_lineBufStart;
	char* m_lineBufEnd;
};

#endif // STREAM_H