//****Note, linux only!
#ifdef _WIN32
# include <assert.h>
  assert(false);
#endif

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  // Needed for getaddrinfo() and freeaddrinfo()
#include <unistd.h> // Needed for close()
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/wireless.h>

#define IS_VALID(s) ((s) > 0)
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "timer.h"
#include "stream.h"
#include "socket.h"

// uncomment to allow socket connections to time out
#define USE_NON_BLOCKING

// detect wifi network windows is using
bool autoDetectWifi(char *ssid, char *password, int &channel)
{
	bool success = false;
	if(ssid && password)
	{
		*ssid = '\0';
		*password = '\0';
		channel = -1;
		iwreq wreq;

		memset(&wreq, 0, sizeof(iwreq));
		sprintf(wreq.ifr_name, "wlan0");
		int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if(sockfd >= 0) 
		{
			char buffer[IW_ESSID_MAX_SIZE];
			buffer[0] = '\0';
			wreq.u.essid.pointer = buffer;
			wreq.u.essid.length = IW_ESSID_MAX_SIZE;
			if(ioctl(sockfd, SIOCGIWESSID, &wreq) >= 0) 
			{
				printf("IOCTL Successfull\n");
				printf("ESSID is %s\n", (const char*)wreq.u.essid.pointer);
			}
		}
		//****FixMe, fill this in
	}

	return success;
}

const char* errorNumToStr(int num)
{
	switch(num)
	{
	case 0: return "No error";
	case EINTR: return "Interrupted system call";
	case EBADF: return "Bad file number";
	case EACCES: return "Permission denied";
	case EFAULT: return "Bad address";
	case EINVAL: return "Invalid argument";
	case EMFILE: return "Too many open sockets";
	case EWOULDBLOCK: return "Operation would block";
	case EINPROGRESS: return "Operation now in progress";
	case EALREADY: return "Operation already in progress";
	case ENOTSOCK: return "Socket operation on non-socket";
	case EDESTADDRREQ: return "Destination address required";
	case EMSGSIZE: return "Message too long";
	case EPROTOTYPE: return "Protocol wrong type for socket";
	case ENOPROTOOPT: return "Bad protocol option";
	case EPROTONOSUPPORT: return "Protocol not supported";
	case ESOCKTNOSUPPORT: return "Socket type not supported";
	case EOPNOTSUPP: return "Operation not supported on socket";
	case EPFNOSUPPORT: return "Protocol family not supported";
	case EAFNOSUPPORT: return "Address family not supported";
	case EADDRINUSE: return "Address already in use";
	case EADDRNOTAVAIL: return "Can't assign requested address";
	case ENETDOWN: return "Network is down";
	case ENETUNREACH: return "Network is unreachable";
	case ENETRESET: return "Net connection reset";
	case ECONNABORTED: return "Software caused connection abort";
	case ECONNRESET: return "Connection reset by peer";
	case ENOBUFS: return "No buffer space available";
	case EISCONN: return "Socket is already connected";
	case ENOTCONN: return "Socket is not connected";
	case ESHUTDOWN: return "Can't send after socket shutdown";
	case ETOOMANYREFS: return "Too many references, can't splice";
	case ETIMEDOUT: return "Connection timed out";
	case ECONNREFUSED: return "Connection refused";
	case ELOOP: return "Too many levels of symbolic links";
	case ENAMETOOLONG: return "File name too long";
	case EHOSTDOWN: return "Host is down";
	case EHOSTUNREACH: return "No route to host";
	case ENOTEMPTY: return "Directory not empty";
//	case EPROCLIM: return "Too many processes";
	case EUSERS: return "Too many users";
	case EDQUOT: return "Disc quota exceeded";
	case ESTALE: return "Stale NFS file handle";
	case EREMOTE: return "Too many levels of remote in path";
//	case SYSNOTREADY: return "Network system is unavailable";
//	case VERNOTSUPPORTED: return "Winsock version out of range";
//	case NOTINITIALISED: return "WSAStartup not yet called";
//	case EDISCON: return "Graceful shutdown in progress";
	case HOST_NOT_FOUND: return "Host not found";
//	case NO_DATA: return "No host data of that type was found";
	}

	return "unknown error";
}

const char* getLastErrorMessage(int errNum = 0)
{
	static char tstr[256];

	if(errNum == 0)
		errNum = errno;
	sprintf(tstr, "%s (%d)", errorNumToStr(errNum), errNum);
	tstr[sizeof(tstr)-1] = '\0';

	return tstr;
}

// given a dotted or host name, get inet_addr
unsigned long lookupAddress(const char* pcHost)
{
	unsigned long addr = INADDR_NONE;

	if(pcHost)
	{
		// try dotted address
		addr = inet_addr(pcHost);
		if (addr == INADDR_NONE) 
		{
			// if that fails try dns
			hostent* pHE = gethostbyname(pcHost);
			if (pHE != 0) 
				addr = *((unsigned long*)pHE->h_addr_list[0]);
		}
	}

	return addr;
}

int Socket::waitOnSocket(int timeout_ms, bool checkWrite)
{
	int ret = -1;
	if(IS_VALID(m_soc)) //really same as isOpen() 
	{
		fd_set fdread;
	    FD_ZERO(&fdread);
	    FD_SET(m_soc, &fdread);

		fd_set fdwrite;
	    FD_ZERO(&fdwrite);
	    FD_SET(m_soc, &fdwrite);

		fd_set fderror;
	    FD_ZERO(&fderror);
	    FD_SET(m_soc, &fderror);

		timeval tv;
	    tv.tv_sec = timeout_ms / 1000;
	    tv.tv_usec = (timeout_ms % 1000) * 1000;

		// ret > 0 indicates an event triggered
		// ret == 0 indicates we hit the timeout
		// ret == -1 indicates error 
		ret = select(m_soc + 1, &fdread, (checkWrite) ? &fdwrite : NULL, &fderror, &tv);
		if(ret > 0)
	    {
			int iResult;
	        socklen_t len = sizeof iResult;
	        ret = getsockopt(m_soc, SOL_SOCKET, SO_ERROR, (char*)&iResult, &len);
			if(ret >= 0)
			{
				if(iResult == 0) 
				{
					debugPrint(DBG_LOG, "waitOnSocket succeeded");
					return 1; // success, connected with no timeout
				}
				else // failed with error
					debugPrint(DBG_WARN, "waitOnSocket failed with error: %s", getLastErrorMessage(iResult));
			}
			else // failed with error
				debugPrint(DBG_WARN, "waitOnSocket getsockopt failed with error: %s", getLastErrorMessage());

			ret = -1;
	    }
		else if(ret == 0)
			debugPrint(DBG_LOG, "waitOnSocket hit timeout, %s", getLastErrorMessage());
		else
			debugPrint(DBG_WARN, "waitOnSocket failed with timeout, %d:%s", ret, getLastErrorMessage());
	}
	else
		debugPrint(DBG_WARN, "waitOnSocket failed not connected to server!");

	return ret;
}

Socket::Socket()
	: m_soc(INVALID_SOCKET)
{
}

Socket::~Socket()
{
	Socket::closeStream();
}

bool Socket::openSocket(const char *ip, int port) 
{
	bool success = false;

	//if(m_isInit)
	{
		// Create a SOCKET for connecting to server
		m_soc = socket(AF_INET, SOCK_STREAM, 0);
		if(IS_VALID(m_soc)) 
		{
#ifdef USE_NON_BLOCKING
			// turn on non blocking mode
			unsigned long arg = 1;
			ioctl(m_soc, FIONBIO, &arg);
#endif
			sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = lookupAddress(ip);
			addr.sin_port = htons(port);

			// Connect the socket to the given IP and port
			if (connect(m_soc, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR
#ifdef USE_NON_BLOCKING
				|| errno == EWOULDBLOCK
				|| errno == EINPROGRESS)
				success = (waitOnSocket(5000, true) > 0);
#else
				)
				success = true;
#endif
			else
				debugPrint(DBG_WARN, "socket connect failed with error: %s", getLastErrorMessage());

			if(!success)
				close(m_soc);
		}
		else
			debugPrint(DBG_WARN, "socket failed with error: %s", getLastErrorMessage());
	}
	//else
	//	debugPrint(DBG_WARN, "winsock not initialized");

	return success;
}

void Socket::closeStream()
{
	if(isOpen()) 
	{
		// drain buffers
		clear();

		// tell server we are exiting
		if(shutdown(m_soc, SHUT_RDWR) != SOCKET_ERROR) 
		{
			// actually close the socket
			if(close(m_soc) != SOCKET_ERROR)
			{
				// success
			}
			else
				debugPrint(DBG_WARN, "close socket failed with error: %s", getLastErrorMessage());
		}
		else
			debugPrint(DBG_WARN, "shutdown failed with error: %s", getLastErrorMessage());
	}
	else
		debugPrint(DBG_LOG, "Not connected to server!");

	m_soc = INVALID_SOCKET;
}

bool Socket::isOpen() 
{ 
	return IS_VALID(m_soc); 
}

void Socket::clear() // is this ever needed?
{
	// call parrent
	Stream::clear();

	if(isOpen())
	{
		//****FixMe, how do we do this?
		/*
		// check if we have data waiting, without stalling
		if(waitOnSocket(50, false) > 0)
		{
			// log any leftover data
			const int len = 4096;
			char buf[len];
			if(read(buf, len))
				debugPrint(DBG_REPORT, "Socket::clear() leftover data: %s", buf);
		}
		*/
	}
}

int Socket::read(char *buf, int len)
{
	int bytesRead = 0;

	if(buf)
	{
		buf[0] = '\0';

		if(isOpen() && len > 0) 
		{
#ifdef USE_NON_BLOCKING
			// wait for data without blocking
			int ret = waitOnSocket(50, false);

			// if data available
			if(1 == ret)
			{
#endif
				int tLen = recv(m_soc, buf, len-1, 0);
				if(tLen != SOCKET_ERROR)
				{
					if(tLen > 0)
					{
						// success
						bytesRead = tLen;

						if(bytesRead > (len-1))
							bytesRead = len-1;
						buf[bytesRead] = '\0';

						debugPrint(DBG_LOG, "Bytes received: %d", tLen);
					}
					else
					{
						debugPrint(DBG_LOG, "Connection closed by peer");
						closeStream();
					}
				}
				else
				{
					debugPrint(DBG_WARN, "read failed with error: %s", getLastErrorMessage());
					closeStream();
				}
#ifdef USE_NON_BLOCKING
			}
			else if(0 == ret)
				debugPrint(DBG_LOG, "read timeout");
			else // -1 == ret
				debugPrint(DBG_WARN, "read errored out");
#endif
		}
		else
			debugPrint(DBG_WARN, "Not connected to server!");
	}

	return bytesRead;
}

int Socket::write(const char *buf, const int len)
{
	int bytesWritten = 0;

	if(isOpen() && buf && len > 0) 
	{
		//****FixMe, sleep is a hack that makes networking work
		// figure out how to get around this!
		Sleep(300);
		int tLen = send(m_soc, buf, len, 0);
		if(tLen != SOCKET_ERROR && tLen > 0)
		{
			// success
			bytesWritten = tLen;
			debugPrint(DBG_LOG, "Bytes sent: %d:%d", len, tLen);
		}
		else
			debugPrint(DBG_WARN, "failed to write bytes: %s", getLastErrorMessage());
	}
	else
		debugPrint(DBG_WARN, "Not connected to server!");

	return bytesWritten;
}
