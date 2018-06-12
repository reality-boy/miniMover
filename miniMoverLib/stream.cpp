#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
# include <SDKDDKVer.h>
# include <Windows.h>
# pragma warning(disable:4996) // live on the edge!
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "timer.h"
#include "debug.h"
#include "stream.h"

void Stream::clear()
{
	int len;
	char tbuf[1024];

	// poll for data
	//len = readLineWait(tbuf, 1024, 0.005f, false);
	len = readLine(tbuf, 1024);
	if(len > 0)
		debugPrint(DBG_REPORT, "leftover data: %s", tbuf);

	// drain residual data in buffer
	len = m_lineBufEnd - m_lineBufStart;
	if(len > 0)
		debugPrint(DBG_REPORT, "leftover data: %s", m_lineBufStart);

	m_lineBuf[0] = '\0';
	m_lineBufStart = m_lineBuf;
	m_lineBufEnd = m_lineBuf;
}

int Stream::readLine(char *buf, int bufLen)
{
	if(buf && bufLen > 0)
	{
		// make sure we return something
		*buf = '\0';

		if(isOpen())
		{
			// setup our counters
			char *bufStart = buf;
			const char *bufEnd = &buf[bufLen-1];

			// check if we already have a newline terminated string
			// do it here so we don't block if data already waiting
			while((bufStart+1) < bufEnd && m_lineBufStart != m_lineBufEnd)
			{
				*bufStart = *m_lineBufStart;
				m_lineBufStart++;

				if(*bufStart == '\n')
				{
					*bufStart = '\0';
					debugPrint(DBG_LOG, "readLine: %s", buf);
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

			// try once more for a newline in buffer
			while((bufStart+1) < bufEnd && m_lineBufStart != m_lineBufEnd)
			{
				*bufStart = *m_lineBufStart;
				m_lineBufStart++;

				if(*bufStart == '\n')
				{
					*bufStart = '\0';
					debugPrint(DBG_LOG, "readLine: %s", buf);
					return bufStart - buf + 1; // length
				}
				bufStart++;
			}
		}
	}

	return 0;
}

int Stream::readLineWait(char *buf, int bufLen, float timeout_s, bool report)
{
	if(buf && bufLen > 0)
	{
		// make sure we return something
		*buf = '\0';

		if(isOpen())
		{
			if(timeout_s < 0)
				timeout_s = getDefaultTimeout();

			float start = msTime::getTime_s();
			float end = start + timeout_s;
			do
			{
				// blocking call, no need to sleep
				int ret = readLine(buf, bufLen);
				if(ret > 0)
					return ret;
			}
			while(msTime::getTime_s() < end);

			if(report)
				debugPrint(DBG_WARN, "readLineWait triggered timeout %0.4f:%0.4f", timeout_s, msTime::getTime_s() - start);
		}
		else
			debugPrint(DBG_WARN, "readLineWait socket not connected");
	}

	return 0;
}

int Stream::writeStr(const char *buf)
{
	if(buf)
	{
		debugPrint(DBG_LOG,"writeStr: %s", buf);
		return write(buf, strlen(buf));
	}

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
		vsnprintf(tstr, tstrLen, fmt, arglist);
		va_end(arglist);

		tstr[tstrLen-1] = '\0';

		return writeStr(tstr);
	}

	return 0;
}