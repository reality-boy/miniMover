//****Note, linux only!
#ifdef _WIN32
# include <assert.h>
  assert(false);
#endif

// based on code from https://github.com/Marzac/rs232
// and http://www.cmrr.umn.edu/~strupp/serial.html
// and http://kirste.userpage.fu-berlin.de/chemnet/use/info/libc/libc_12.html
// and http://www.unixwiz.net/techtips/termios-vmin-vtime.html

#define __USE_MISC // For CRTSCTS

#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "debug.h"
#include "stream.h"
#include "serial.h"

//------------------------

int SerialHelper::m_portCount = 0;
int SerialHelper::m_defaultPortID = -1;
SerialHelper::PortInfo SerialHelper::portInfo[SerialHelper::m_maxPortCount] = {0};

int SerialHelper::queryForPorts(const char *hint)
{
	debugPrint(DBG_LOG, "SerialHelper::queryForPorts(%s)", hint);

	isdigit('c');
	m_portCount = 0;
	m_defaultPortID = -1;

    struct dirent *ent;
    DIR *dir = opendir("/dev");
    while((ent = readdir(dir)) && m_portCount < m_maxPortCount) 
	{
        if(0 == strncmp("tty", ent->d_name, 3)) 
		{
			// reject /dev/tty, those are terminals not serial ports
			if(ent->d_name[3] != '\0' && !isdigit(ent->d_name[3]))
			{
	            sprintf(portInfo[m_portCount].deviceName, "/dev/%s", ent->d_name);

			    int handle = open(portInfo[m_portCount].deviceName, O_RDWR | O_NOCTTY | O_NDELAY /*| O_NONBLOCK*/);
				if(handle >= 0)
				{
					//printf("found %s\n", portInfo[m_portCount].deviceName);
					//serial_struct serinfo;
					//if(0 == ioctl(handle, TIOCGSERIAL, &serinfo))
					{
						//****FixMe, find proper display name somehow
			            sprintf(portInfo[m_portCount].displayName, "%s", ent->d_name);
						//****FixMe, search for device
						m_defaultPortID = m_portCount;
						m_portCount++;
					}
				}
			}
        }
    }
    closedir(dir);

	return m_defaultPortID;
}

//------------------------

Serial::Serial() 
	: m_handle(-1)
	, m_baudRate(-1)
{
	debugPrint(DBG_LOG, "Serial::Serial()");

	m_deviceName[0] = '\0';
}

Serial::~Serial()
{
	debugPrint(DBG_LOG, "Serial::~Serial()");

	closeStream();
}

int baudFlag(int BaudRate)
{
    switch(BaudRate)
    {
        case 50:      return B50; break;
        case 110:     return B110; break;
        case 134:     return B134; break;
        case 150:     return B150; break;
        case 200:     return B200; break;
        case 300:     return B300; break;
        case 600:     return B600; break;
        case 1200:    return B1200; break;
        case 1800:    return B1800; break;
        case 2400:    return B2400; break;
        case 4800:    return B4800; break;
        case 9600:    return B9600; break;
        case 19200:   return B19200; break;
        case 38400:   return B38400; break;
        case 57600:   return B57600; break;
        case 115200:  return B115200; break;
        case 230400:  return B230400; break;
        default : return B0; break;
    }
}

bool Serial::openStream(const char *deviceName, int baudRate)
{
	debugPrint(DBG_LOG, "Serial::openStream(%s, %d)", deviceName, baudRate);

	assert(deviceName);
	assert(baudRate > 0);

	if(deviceName)
	{
		// if already connected just return
		if(0 == strcmp(deviceName, m_deviceName) && 
			m_baudRate == baudRate)
			return true;

		// close out any previous connection
		closeStream();

		// Open port
	    m_handle = open(deviceName, O_RDWR | O_NOCTTY);
	    if(isOpen())
		{
			// General configuration
		    struct termios options;
		    tcgetattr(m_handle, &options);

			// don't take control of the port from the os
			options.c_cflag |= CLOCAL;

			// enable reading from port
			options.c_cflag |= CREAD;

			// 8 bit
			options.c_cflag &= ~CSIZE;
			options.c_cflag |= CS8;

			// no parity
			options.c_cflag &= ~PARENB;
			options.c_cflag &= ~CSTOPB;

			// no hardware flow control
#ifdef CNEW_RTSCTS
			options.c_cflag &= ~CNEW_RTSCTS;
#endif
#ifdef CRTSCTS
			options.c_cflag &= ~CRTSCTS;
#endif

			// no software flow control
			options.c_iflag &= ~(IXON | IXOFF | IXANY);

			// baudrate, translating from a number to Bflag
		    int flag = baudFlag(baudRate);
		    cfsetispeed(&options, flag);
		    cfsetospeed(&options, flag);

			// raw mode, not canonical
			// that is don't buffer till newline
			options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

			// raw output, don't post process outgoing strings
			options.c_oflag &= ~OPOST;

			// Timeouts configuration
			// wait time in 10th of a second
		    options.c_cc[VTIME] = 1; 

			// minimum number of characters to read, 0 is no minimum
		    options.c_cc[VMIN]  = 0; 

			// used to setup blocking/non blocking mode
			// redundant, already set at open
		    //fcntl(m_handle, F_SETFL, 0);

			// Validate configuration
		    if(tcsetattr(m_handle, TCSANOW, &options) >= 0) 
			{
				return true;
			}
			else
				debugPrint(DBG_WARN, "Serial::openStream tcsetattr failed with error %d", errno);
		}
		else
			debugPrint(DBG_WARN, "Serial::openStream open failed with error %d", errno);
	}
	else
		debugPrint(DBG_WARN, "Serial::openStream failed invalid input");

	// Close if already open
	closeStream();

	return false;
}

void Serial::closeStream()
{
	debugPrint(DBG_LOG, "Serial::closeStream()");

	if(isOpen())
	{
		// drain buffers
		Stream::clear();

		tcdrain(m_handle);
		close(m_handle);
	}
	else
		debugPrint(DBG_VERBOSE, "Serial::closeStream failed invalid connection");

	m_handle = -1;
	m_baudRate = -1;
	m_deviceName[0] = '\0';
}

bool Serial::isOpen()
{
	debugPrint(DBG_VERBOSE, "Serial::isOpen()");

	return m_handle >= 0;
}

void Serial::clear()
{
	debugPrint(DBG_VERBOSE, "Serial::clear()");

	// call parent
	Stream::clear();

	if(isOpen())
	{
		// check if data waiting, without stalling
		int bytes;
		ioctl(m_handle, FIONREAD, &bytes);
		if(bytes > 0)
		{
			// log any leftover data
			const int len = 4096;
			char buf[len];
			if(read(buf, len))
				debugPrint(DBG_REPORT, "Serial::clear leftover data: '%s'", buf);
		}
	}
	else
		debugPrint(DBG_WARN, "Serial::clear failed invalid connection");
}

int Serial::read(char *buf, int len)
{
	debugPrint(DBG_VERBOSE, "Serial::read()");

	assert(buf);
	assert(len > 0);

	if(buf && len > 0)
	{
		buf[0] = '\0';

		if(isOpen())
		{
#if 0 
			// debug path, dolls out data in small chuncks
			static int tbufBytes = 0;
			static char tbuf[512];
			if(tbufBytes > 0)
			{
				int l = min(34, min(len, tbufBytes));
				strncpy(buf, tbuf, l);
				buf[l] = '\0';

				tbufBytes -= l;
				if(tbufBytes > 0)
					memcpy(tbuf, tbuf+l, tbufBytes);

				debugPrint(DBG_LOG, "Serial::read cache returned %d bytes", l);

				return l;
			}
			else
			{
				memset(tbuf, 0, sizeof(tbuf));

				int tLen = ::read(m_handle, tbuf, sizeof(tbuf));
				if(tLen > 0)
				{
					tbufBytes = tLen;
					int l = min(34, min(len, tbufBytes));
					strncpy(buf, tbuf, l);
					buf[l] = '\0';

					tbufBytes -= l;
					if(tbufBytes > 0)
						memcpy(tbuf, tbuf+l, tbufBytes);

					debugPrint(DBG_LOG, "Serial::read returned %d bytes", l);

					return l;
				}
				else if(tLen == 0)
				{
					// no data yet
				}
				else
				{
					debugPrint(DBG_WARN, "Serial::read failed with error %d", errno);
					closeStream();
				}
			}
#else
			int tLen = ::read(m_handle, buf, len-1);
			if(tLen > 0)
			{
				// success
				buf[tLen] = '\0';
				debugPrint(DBG_LOG, "Serial::read returned %d bytes", tLen);
				return tLen;
			}
			else if(tLen == 0)
			{
				// no data yet
			}
			else
			{
				debugPrint(DBG_WARN, "Serial::read failed with error %d", errno);
				closeStream();
			}
#endif
		}
		else
			debugPrint(DBG_WARN, "Serial::read failed invalid connection");
	}
	else
		debugPrint(DBG_WARN, "Serial::read failed invalid input");

	return 0;
}

int Serial::write(const char *buf, int len)
{
	debugPrint(DBG_VERBOSE, "Serial::write(%s, %d)", buf, len);

	assert(buf);
	assert(len > 0);

	if(buf && len > 0)
	{
		if(isOpen())
		{
			int tLen = ::write(m_handle, buf, len);
			if(tLen > 0)
			{
				// success
				debugPrint(DBG_LOG, "Serial::write sent: %d of %d bytes", tLen, len);
				return tLen;
			}
			else
			{
				debugPrint(DBG_WARN, "Serial::write failed with error %d", errno);
				closeStream();
			}
		}
		else
			debugPrint(DBG_WARN, "Serial::write failed invalid connection");
	}
	else
		debugPrint(DBG_WARN, "Serial::write failed invalid input");

	return 0;
}
