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
#include <ctype.h>
#include <assert.h>

#include "timer.h"
#include "debug.h"
#include "stream.h"

void StreamT::clear()
{
	debugPrint(DBG_LOG, "StreamT::clear()");

	// drain residual data in readline buffer
	if(m_lineBufCount > 0)
		debugPrint(DBG_REPORT, "StreamT::clear leftover data: '%s'", m_lineBuf);

	m_lineBuf[0] = '\0';
	m_lineBufCount = 0;

	// assume child class will take care of the device speciffic data
}

bool StreamT::isLineInBuffer()
{
	debugPrint(DBG_VERBOSE, "stream::doesBufferHaveLine()");

	for(int i=0; i<m_lineBufCount; i++)
	{
		if(m_lineBuf[i] == '\n')
			return true;
	}

	return false;
}

int StreamT::readLineFromBuffer(char *buf, int bufLen, bool readPartial)
{
	debugPrint(DBG_VERBOSE, "stream::readlinefrombuffer()");

	assert(buf);
	assert(bufLen > 0);

	if(buf && bufLen > 0)
	{
		// make sure we return something
		*buf = '\0';

		int i;
		for(i=0; i<m_lineBufCount; i++)
		{
			if(m_lineBuf[i] == '\n')
				break;
		}

		// if found, copy
		// if connection closed then return what we have
		if(i < m_lineBufCount || !isOpen() || readPartial)
		{
			int len = i;
			if(len >= bufLen)
			{
				debugPrint(DBG_WARN, "StreamT::readLineFromBuffer data buffer too small, increase by %d bytes", len - bufLen);
				len = bufLen-1;
			}

			if(len > 0)
			{
				strncpy(buf, m_lineBuf, len);
				buf[len] = '\0';

				m_lineBufCount -= len+1;
				if(m_lineBufCount > 0)
				{
					for(int i=0; i<m_lineBufCount; i++)
						m_lineBuf[i] = m_lineBuf[i+len+1];
				}
				else
					m_lineBufCount = 0;
				m_lineBuf[m_lineBufCount] = '\0';

				return len;
			}
		}
	}
	else
		debugPrint(DBG_WARN, "StreamT::readLineFromBuffer failed invalid input");

	return 0;
}

int StreamT::readLine(char *buf, int bufLen, bool readPartial)
{
	debugPrint(DBG_VERBOSE, "StreamT::readLine()");

	assert(buf);
	assert(bufLen > 0);

	if(buf && bufLen > 0)
	{
		// make sure we return something
		*buf = '\0';

		int len;

		// check if we already have a newline terminated string
		// do it here so we don't block if data already waiting
		len = readLineFromBuffer(buf, bufLen, readPartial);
		if(len > 0)
		{
			debugPrint(DBG_LOG, "StreamT::readLine returned '%s'", buf);
			return len;
		}
		else // not found, pull more data
		{
			if(isOpen())
			{
				len = m_lineBufLen - m_lineBufCount - 1;
				len = read(m_lineBuf + m_lineBufCount, len);
				if(len > 0)
				{
					m_msgTimer.stopTimer();
					m_lineBufCount += len;

					len = readLineFromBuffer(buf, bufLen, readPartial);
					if(len > 0)
					{
						debugPrint(DBG_LOG, "StreamT::readLine returned '%s'", buf);
						return len;
					}
				}
			}
			else
				debugPrint(DBG_WARN, "StreamT::readLine failed invalid connection");
		}
	}
	else
		debugPrint(DBG_WARN, "StreamT::readLine failed invalid input");
		
	return 0;
}

int StreamT::writeStr(const char *buf)
{
	debugPrint(DBG_VERBOSE, "StreamT::writeStr(%s)", buf);

	assert(buf);

	if(buf)
	{
		debugPrint(DBG_LOG,"StreamT::writeStr sent %s", buf);
		m_msgTimer.startTimer();
		return write(buf, strlen(buf));
	}
	else
		debugPrint(DBG_WARN, "StreamT::writeStr failed invalid input");

	return 0;
}

int StreamT::writePrintf(const char *fmt, ...)
{
	debugPrint(DBG_VERBOSE, "StreamT::writePrintf(%s)", fmt);

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
		debugPrint(DBG_WARN, "StreamT::writePrintf failed invalid input");

	return 0;
}

bool StreamT::isNetworkAddress(const char *addr)
{
	// auto detect serial port if no addres defined
	if(!addr || !addr[0] || (addr[0] == '-' && addr[1] == '1'))
		return false;

	// linux serial port
	if(0 == strncmp("/dev/tty", addr, strlen("/dev/tty")) ||
	   0 == strncmp("tty", addr, strlen("tty")) ) // allow to skip /dev/
		return false;

	// windows serial port
	if(0 == strncmp(addr, "\\\\.\\COM", strlen("\\\\.\\COM")) ||
	   0 == strncmp(addr, "COM", strlen("COM")) ||
	   0 == strncmp(addr, "com", strlen("com")) || // lower case is invalid, but logical
	   0 == strncmp(addr, "port", strlen("port")) ) // not really a windows port
		return false;

	// is a simple number
	if((isdigit(*addr) && strlen(addr) < 4) || // is a single number
	    strlen(addr) < 8) // assume short strings are com ports
		return false;

	// else assume some sort of ip or dns name
	return true;
}
