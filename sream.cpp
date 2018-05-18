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
	lineBuf[0] = '\0';
	lineBufStart = lineBuf;
	lineBufEnd = lineBuf;
}

//****FixMe, this does not work right at all.  If we ever recieved a
// partial line we would never fill it in properly and recieve the rest of 
// the line.  It only works if we recieve one or more full lines at a time.
// yuck!
int Stream::readLine(char *buf, int bufLen)
{
	int len = 0;
	if(buf && bufLen > 0)
	{
		*buf = '\0';
		char *bufStart = buf;
		const char *bufEnd = &buf[bufLen-1];

		//loop around at least twicw in order to ensure we drained the buffer and found a new line
		for(int i=0; i<2; i++)
		{
			if(lineBufStart == lineBufEnd)
			{
				lineBufStart = lineBuf;
				len = read(lineBuf, lineBufLen);
				lineBufEnd = &lineBuf[len];
			}

			if(lineBufStart != lineBufEnd)
			{
				bool isDone = false;
				while(bufStart != bufEnd && lineBufStart != lineBufEnd)
				{
					*bufStart = *lineBufStart;

					if(*bufStart == '\n')
					{
						*bufStart = '\0';
						isDone = true;
					}

					bufStart++;
					lineBufStart++;

					if(isDone)
					{
						len = bufStart - buf;

						if(len > 0)
							debugPrint(DBG_LOG, "recieved: %s", buf);

						return len;
					}
				}
			}
		}

		*bufStart = '\0';
		len = bufStart - buf;

		if(len > 0)
			debugPrint(DBG_LOG, "recieved partial: %s", buf);

		return len;
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