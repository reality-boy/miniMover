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
#include "network.h"

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

bool waitOnSocketConnect(int soc, int timeout_ms)
{
	fd_set fdread;
    FD_ZERO(&fdread);
    FD_SET(soc, &fdread);

	fd_set fdwrite;
    FD_ZERO(&fdwrite);
    FD_SET(soc, &fdwrite);

	timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

	int res = select(soc + 1, &fdread, &fdwrite, NULL, &tv);
	if(res == 1)
    {
		int iResult;
        socklen_t len = sizeof iResult;
        getsockopt(soc, SOL_SOCKET, SO_ERROR, (char*)&iResult, &len);

		if(iResult != SOCKET_ERROR) 
			return true; // success, connected with no timeout
		else // failed with error
			debugPrint(DBG_WARN, "wait failed with error: %s", getLastErrorMessage());
    }
	else
		debugPrint(DBG_LOG, "wait failed with timeout, %d:%s", res, getLastErrorMessage());

	return false;
}

// -1 - error, 0 - timeout, 1 - data ready
int waitOnSocketReciev(int soc, int timeout_ms)
{
	fd_set fdread;
    FD_ZERO(&fdread);
    FD_SET(soc, &fdread);

	timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

	return select(soc + 1, &fdread, NULL, NULL, &tv);
}

Socket::Socket()
	: soc(INVALID_SOCKET)
{
	isInit = true;
}

Socket::~Socket()
{
	if(isInit)
		Socket::closeStream();
	isInit = false;
}

bool Socket::openSocket(const char *ip, int port) 
{
	bool success = false;
	int iResult;

	if(isInit)
	{
		addrinfo hints;
		addrinfo *adrInf = NULL;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		//hints.ai_protocol = IPPROTO_TCP;

		char portStr[32];
		sprintf(portStr, "%d", port);

		iResult = getaddrinfo(ip, portStr, &hints, &adrInf);
		if(iResult == 0) 
		{
			// Attempt to connect to an address until one succeeds
			// in our case this could be either an ipv4 or ipv6 address
			for(addrinfo *pInfo = adrInf; pInfo != NULL; pInfo = pInfo->ai_next) 
			{
				// Create a SOCKET for connecting to server
				soc = socket(pInfo->ai_family, pInfo->ai_socktype, pInfo->ai_protocol);
				if(IS_VALID(soc)) 
				{
					// turn on non blocking mode
					unsigned long arg = 1;
					ioctl(soc, FIONBIO, &arg);

					// Connect to server.
					iResult = connect(soc, pInfo->ai_addr, (int)pInfo->ai_addrlen);
					/*
					if(iResult != SOCKET_ERROR) 
						break; // success we found a connection
					else
						debugPrint(DBG_WARN, "connect failed with error: %s", getLastErrorMessage());
					*/

					if(waitOnSocketConnect(soc, 5000))
						break;

					closeStream();
				}
				else
					debugPrint(DBG_WARN, "socket failed with error: %s", getLastErrorMessage());
			}
			freeaddrinfo(adrInf);

			if(IS_VALID(soc)) 
			{
				usleep(500 * 1000); // spin for a bit
				success = true;
			}
			else
				debugPrint(DBG_WARN, "Unable to connect to server!");
		}
		else
			debugPrint(DBG_WARN, "getaddrinfo failed with error: %d:%s", iResult, gai_strerror(iResult));
	}
	else
		debugPrint(DBG_WARN, "winsock not initialized");

	return success;
}

void Socket::closeStream()
{
	if(isInit && IS_VALID(soc)) 
	{
		// tell server we are exiting
		if(shutdown(soc, SHUT_WR) != SOCKET_ERROR) 
		{
			// wait for close signal, and drain connection
			char tbuf[1024];
			while(true)
			{
				int bytes = recv(soc, tbuf, 1024, 0);
				if (bytes == SOCKET_ERROR)
				{
					//debugPrint(DBG_WARN, "close socket drain failed!");
					//return false;
					break;
				}
				else if (bytes != 0)
					debugPrint(DBG_WARN, "extra bytes recieved at close: %d", bytes);
				else
					break;
			}

			// actually close the socket
			if(close(soc) != SOCKET_ERROR)
			{
				// success
			}
			else
				debugPrint(DBG_WARN, "close socket failed!");
		}
		else
			debugPrint(DBG_LOG, "shutdown failed!");
	}
	else
		debugPrint(DBG_LOG, "Not connected to server!");

	soc = INVALID_SOCKET;
}

bool Socket::isOpen() 
{ 
	return isInit && IS_VALID(soc); 
}

void Socket::clear() // is this ever needed?
{
	// call parrent
	Stream::clear();

	if(isInit && IS_VALID(soc))
	{
		// check if we have data waiting, without stalling
		if(1 == waitOnSocketReciev(soc, 50))
		{
			// log any leftover data
			const int len = 4096;
			char buf[len];
			if(read(buf, len))
				debugPrint(DBG_REPORT, "leftover data: %s", buf);
		}
	}
}

int Socket::read(char *buf, int len)
{
	int bytesRead = 0;

	if(buf)
	{
		*buf = '\0';
		if(isInit && IS_VALID(soc) && len > 0) 
		{
			if(1 == waitOnSocketReciev(soc, 50))
			{
				int tLen = recv(soc, buf, len-1, 0);
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
						debugPrint(DBG_LOG, "Connection closed by peer");
				}
				else
					debugPrint(DBG_WARN, "read failed with error: %s", getLastErrorMessage());
			}
			//else
			//	debugPrint(DBG_WARN, "read timeout");
		}
		else
			debugPrint(DBG_LOG, "Not connected to server!");
	}

	return bytesRead;
}

int Socket::write(const char *buf, const int len)
{
	int bytesWritten = 0;

	if(isInit && IS_VALID(soc) && buf && len > 0) 
	{
		int tLen = send(soc, buf, len, 0);
		if(tLen != SOCKET_ERROR)
		{
			// success
			bytesWritten = tLen;
			debugPrint(DBG_LOG, "Bytes sent: %d:%d", len, tLen);
		}
		else
			debugPrint(DBG_WARN, "failed to write bytes: %s", getLastErrorMessage());
	}

	return bytesWritten;
}
