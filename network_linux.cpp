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


bool getXMLValue(const char *xml, const char* key, char *val)
{
	if(val)
	{
		*val = '\0';

		if(key && xml)
		{
			const char *t = strstr(xml, key);
			if(t)
			{
				t += strlen(key);
				while(t && *t != '<')
				{
					*val = *t;
					t++;
					val++;
				}

				*val = '\0';

				return true;
			}
		}
	}

	return false;
}

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

Socket::Socket()
	: soc(INVALID_SOCKET)
{
	isInit = true;
}

Socket::~Socket()
{
	if(isInit)
		Socket::closeSocket();
	isInit = false;
}

bool Socket::openSocket(const char *ip, int port) 
{
	bool success = false;
	int iResult;

	if(isInit)
	{
		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		// Resolve the server address and port
		char portStr[32];
		sprintf(portStr, "%d", port);
		addrinfo *adrInf = NULL;
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
					// Connect to server.
					//****FixMe, deal with blocking?
					iResult = connect(soc, pInfo->ai_addr, (int)pInfo->ai_addrlen);
					if(iResult != SOCKET_ERROR) 
					{
						break; // success we found a connection
					}

					Socket::closeSocket();
				}
				else
					debugPrint(DBG_WARN, "socket failed with error: %ld\n", errno);
			}
			freeaddrinfo(adrInf);

			if(IS_VALID(soc)) 
			{
				success = true;
			}
			else
				debugPrint(DBG_WARN, "Unable to connect to server!\n");
		}
		else
			debugPrint(DBG_WARN, "getaddrinfo failed with error: %d\n", iResult);
	}
	else
		debugPrint(DBG_WARN, "winsock not initialized\n");

	return success;
}

bool Socket::closeSocket()
{
	bool success = false;

	if(isInit && IS_VALID(soc)) 
	{
		shutdown(soc, SHUT_RDWR);
		close(soc);

		soc = INVALID_SOCKET;
		success = true;
	}
	else
		debugPrint(DBG_WARN, "Not connected to server!\n");

	return success;
}

bool Socket::isOpen() 
{ 
	return isInit && IS_VALID(soc); 
}

void Socket::clear() // is this ever needed?
{
	// call parrent
	Stream::clear();

	// check if we have data waiting, without stalling
	//****FixMe, check if data before calling read()
	{
		// log any leftover data
		const int len = 4096;
		char buf[len];
		if(read(buf, len))
			debugPrint(DBG_REPORT, "leftover data: %s", buf);
	}
}

int Socket::read(char *buf, int len)
{
	int count = -1;

	if(buf)
	{
		*buf = '\0';

		if(isInit && IS_VALID(soc)) 
		{
			int iResult = -1;
			//****FixMe, deal with blocking
			iResult = recv(soc, buf, len, 0);
			if(iResult >= 0)
			{
				debugPrint(DBG_LOG, "Bytes received: %d\n", iResult);
				count = iResult;
			}
			else
				debugPrint(DBG_WARN, "recv failed with error: %d\n", errno);
		}
		else
			debugPrint(DBG_WARN, "Not connected to server!\n");
	}

	return count;
}

int Socket::write(const char *buf, const int len)
{
	int bytesWritten = 0;

	if(isInit && IS_VALID(soc)) 
	{
		// Send an initial buffer
		//****FixMe, do we need to loop to send it all?
		//****FixMe, deal with blocking
		bytesWritten = send(soc, buf, len, 0);
		if(bytesWritten != SOCKET_ERROR)
		{
			debugPrint(DBG_LOG, "Bytes Sent: %ld\n", bytesWritten);

			//if(bytesWritten == strlen(buf))
		}
		else
			debugPrint(DBG_WARN, "send failed with error: %d\n", errno);
	}
	else
		debugPrint(DBG_WARN, "Not connected to server!\n");

	return bytesWritten;
}

