#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
//#include <Setupapi.h>
#include <assert.h>
//#include <winioctl.h>

#include "debug.h"
#include "stream.h"

void Stream::clear()
{
	// log any leftover data
	int len = m_lineBufEnd - m_lineBufStart;
	if(len > 0)
		debugPrint(DBG_REPORT, "leftover data: %s", m_lineBufStart);

	m_lineBuf[0] = '\0';
	m_lineBufStart = m_lineBuf;
	m_lineBufEnd = m_lineBuf;
}

//****FixMe, this does not work right at all.  If we ever recieved a
// partial line we would never fill it in properly and recieve the rest of 
// the line.  It only works if we recieve one or more full lines at a time.
// yuck!
int Stream::readLine(char *buf, int bufLen)
{
	if(buf && bufLen > 0)
	{
		// make sure we return something
		*buf = '\0';

		// setup our counters
		char *bufStart = buf;
		const char *bufEnd = &buf[bufLen-1];

		// check if we already have a newline terminated string
		while((bufStart+1) < bufEnd && m_lineBufStart != m_lineBufEnd)
		{
			*bufStart = *m_lineBufStart;
			m_lineBufStart++;

			if(*bufStart == '\n')
			{
				*bufStart = '\0';
				debugPrint(DBG_LOG, "recieved: %s", buf);
				return bufStart - buf + 1; // length
			}
			bufStart++;
		}

		int len = 0;
		// move old data to start of buffer
		if(m_lineBufStart < m_lineBufEnd)
		{
			len = m_lineBufEnd - m_lineBufStart;
			memcpy(m_lineBuf, m_lineBufStart, len);
			m_lineBufStart = m_lineBuf;
			m_lineBufEnd = m_lineBuf + len;
		}

		// get new data from serial device
		len = (m_lineBuf + m_lineBufLen) - m_lineBufStart;
		len = read(m_lineBufEnd, len);
		m_lineBufEnd += len;

		// try once more for a newline
		while((bufStart+1) < bufEnd && m_lineBufStart != m_lineBufEnd)
		{
			*bufStart = *m_lineBufStart;
			m_lineBufStart++;

			if(*bufStart == '\n')
			{
				*bufStart = '\0';
				debugPrint(DBG_LOG, "recieved: %s", buf);
				return bufStart - buf + 1; // length
			}
			bufStart++;
		}

		// null terminate the string
		*bufStart = '\0';

		// log the error
		debugPrint(DBG_LOG, "recieved partial: %s", buf);

		// but return true if we got a partial string
		return bufStart - buf; // length
	}

	return 0;
}

int Stream::writeStr(const char *buf)
{

	if(buf)
		return write(buf, strlen(buf));

	return 0;
}

int Stream::writePrintf(const char *fmt, ...)
{
	if(fmt)
	{
		static const int tstrLen = 4096;
		char tstr[tstrLen];

		va_list arglist;
		va_start(arglist, fmt);

		//customized operations...
		vsnprintf_s(tstr, tstrLen, fmt, arglist);
		tstr[tstrLen-1] = '\0';

		va_end(arglist);

		return writeStr(tstr);
	}

	return 0;
}