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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

#include <sys/ioctl.h>

#include "debug.h"
#include "stream.h"
#include "serial.h"

//---------------------

int SerialHelper::portCount = 0;
int SerialHelper::defaultPortID = -1;
SerialHelper::PortInfo SerialHelper::portInfo[maxPortCount] = {0};

int SerialHelper::queryForPorts(const char *hint)
{
	isdigit('c');
	portCount = 0;
	defaultPortID = -1;

    struct dirent *ent;
    DIR *dir = opendir("/dev");
    while((ent = readdir(dir)) && portCount < maxPortCount) 
	{
        if(0 == strncmp("tty", ent->d_name, 3)) 
		{
			// reject /dev/tty, those are terminals not serial ports
			if(ent->d_name[3] != '\0' && !isdigit(ent->d_name[3]))
			{
	            sprintf(portInfo[portCount].deviceName, "/dev/%s", ent->d_name);

			    int handle = open(portInfo[portCount].deviceName, O_RDWR | O_NOCTTY | O_NDELAY /*| O_NONBLOCK*/);
				if(handle >= 0)
				{
					//printf("found %s\n", portInfo[portCount].deviceName);
					//serial_struct serinfo;
					//if(0 == ioctl(handle, TIOCGSERIAL, &serinfo))
					{
						//****FixMe, find proper display name somehow
			            sprintf(portInfo[portCount].displayName, "%s", ent->d_name);
						//****FixMe, search for device
						defaultPortID = portCount;
						portCount++;
					}
				}
			}
        }
    }
    closedir(dir);

	return defaultPortID;
}

//------------------

Serial::Serial() 
{
	m_handle = -1;
	m_deviceName[0] = '\0';
	m_baudRate = -1;
}

Serial::~Serial()
{
	closeSerial();
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

bool Serial::openSerial(const char *deviceName, int baudRate)
{
	// Close if already open
	closeSerial();

	// Open port
    m_handle = open(deviceName, O_RDWR | O_NOCTTY);
    if(m_handle >= 0)
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

        close(m_handle);
	}
	m_handle = -1;
	return false;
}

void Serial::closeSerial()
{
    if(m_handle > 0)
	{
	    tcdrain(m_handle);
	    close(m_handle);
	    m_handle = -1;
	}
}

bool Serial::isOpen()
{
	return m_handle >= 0;
}

void Serial::clear()
{
	// call parrent
	Stream::clear();

	if(m_handle >= 0)
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
				debugPrint(DBG_REPORT, "leftover data: %s", buf);
		}
	}
}

int Serial::read(char *buf, int len)
{
	int bytesRead = 0;

	if(buf)
	{
		buf[0] = '\0';
	    if(m_handle >= 0 && len > 0)
		{
		    int tLen = ::read(m_handle, buf, len-1);
		    if (tLen > 0)
			{
				// success
				bytesRead = tLen;

				if(bytesRead > (len-1))
					bytesRead = len-1;
				buf[bytesRead] = '\0';

				//debugPrint(DBG_LOG, "Bytes received: %d - %s", tLen, buf);
				debugPrint(DBG_LOG, "Bytes received: %d", tLen);
				debugPrintArray(DBG_VERBOSE, buf, tLen);
			}
			else
				debugPrint(DBG_WARN, "read failed");
		}
	}

	return bytesRead;
}

int Serial::write(const char *buf, int len)
{
	int bytesWritten = 0;

    if(m_handle >= 0 && buf && len > 0)
	{
	    int tLen = ::write(m_handle, buf, len);
	    if(tLen > 0)
		{
			// success
			bytesWritten = tLen;
			//if(buf[len-1] == '\0')
			//	debugPrint(DBG_LOG, "Bytes sent: %d:%d - %s", len, tLen, buf);
			//else
				debugPrint(DBG_LOG, "Bytes sent: %d:%d", len, tLen);
			debugPrintArray(DBG_VERBOSE, buf, len);
		}
		else
			debugPrint(DBG_ERR, "failed to write bytes: %d", tLen);
	}

	return bytesWritten;
}
