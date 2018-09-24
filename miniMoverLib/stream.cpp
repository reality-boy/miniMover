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
	debugPrint(DBG_VERBOSE, "Stream::clear()");

	// drain residual data in readline buffer
	if(m_lineBufCount > 0)
		debugPrint(DBG_REPORT, "Stream::clear leftover data: %s", m_lineBuf);

	m_lineBuf[0] = '\0';
	m_lineBufCount = 0;

	// assume child class will take care of the device speciffic data
}

int Stream::readLineFromBuffer(char *buf, int bufLen)
{
	debugPrint(DBG_VERBOSE, "stream::readlinefrombuffer()");

	assert(buf);
	assert(bufLen > 0);

	if(buf && bufLen > 0)
	{
		int i;
		for(i=0; i<m_lineBufCount; i++)
		{
			if(m_lineBuf[i] == '\n')
				break;
		}

		// if found, copy
		if(i < m_lineBufCount)
		{
			int len = i;
			if(len > bufLen)
			{
				debugPrint(DBG_WARN, "Stream::readLineFromBuffer data buffer too small, increase by %d bytes", len - bufLen);
				len = bufLen;
			}

			strncpy(buf, m_lineBuf, len);
			buf[len] = '\0';
			m_lineBufCount -= len+1;
			if(m_lineBufCount > 0)
				memcpy(m_lineBuf, m_lineBuf+len+1, m_lineBufCount);
			else
				m_lineBufCount = 0;

			return len;
		}
	}
	else
		debugPrint(DBG_WARN, "Stream::readLineFromBuffer failed invalid input");

	return 0;
}

int Stream::readLine(char *buf, int bufLen)
{
	debugPrint(DBG_VERBOSE, "Stream::readLine()");

	assert(buf);
	assert(bufLen > 0);

	if(buf && bufLen > 0)
	{
		// make sure we return something
		*buf = '\0';

		if(isOpen())
		{
			int len;

			// check if we already have a newline terminated string
			// do it here so we don't block if data already waiting
			len = readLineFromBuffer(buf, bufLen);
			if(len > 0)
			{
				debugPrint(DBG_LOG, "Stream::readLine returned '%s'", buf);
				return len;
			}
			else // not found, pull more data
			{
				len = m_lineBufLen - m_lineBufCount;
				len = read(m_lineBuf + m_lineBufCount, len);
				if(len > 0)
				{
					m_lineBufCount += len;

					len = readLineFromBuffer(buf, bufLen);
					if(len > 0)
					{
						debugPrint(DBG_LOG, "Stream::readLine returned '%s'", buf);
						return len;
					}
				}
			}
		}
		else
			debugPrint(DBG_WARN, "Stream::readLine failed invalid connection");
	}
	else
		debugPrint(DBG_WARN, "Stream::readLine failed invalid input");
		
	return 0;
}

int Stream::writeStr(const char *buf)
{
	debugPrint(DBG_VERBOSE, "Stream::writeStr(%s)", buf);

	assert(buf);

	if(buf)
	{
		debugPrint(DBG_LOG,"Stream::writeStr sent %s", buf);
		return write(buf, strlen(buf));
	}
	else
		debugPrint(DBG_WARN, "Stream::writeStr failed invalid input");

	return 0;
}

int Stream::writePrintf(const char *fmt, ...)
{
	debugPrint(DBG_VERBOSE, "Stream::writePrintf(%s)", fmt);

	assert(fmt);

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
	else
		debugPrint(DBG_WARN, "Stream::writePrintf failed invalid input");

	return 0;
}
