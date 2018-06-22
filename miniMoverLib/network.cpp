//****Note, windows only!
#ifndef _WIN32
# include <assert.h>
  assert(false);
#endif

# define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <Windows.h>
#include <Wlanapi.h>
#pragma comment(lib, "Wlanapi.lib")
#include <Winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4996)
#define IS_VALID(s) ((s) != INVALID_SOCKET)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "stream.h"
#include "network.h"

// uncomment to enable non blocking timeouts
//#define NON_BLOCKING

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

		DWORD ver;
		HANDLE handle;
		if(ERROR_SUCCESS == WlanOpenHandle(WLAN_API_MAKE_VERSION(2,0), NULL, &ver, &handle))
		{
			PWLAN_INTERFACE_INFO_LIST ilist;
			if(ERROR_SUCCESS == WlanEnumInterfaces(handle, NULL, &ilist))
			{
				//for(int i=0; i<(int)ilist->dwNumberOfItems; i++)
				// stash off first wifi network found
				if(ilist->dwNumberOfItems > 0)
				{
					int i = 0;
					bool isConnected = (ilist->InterfaceInfo[i].isState == wlan_interface_state_connected);
					GUID *guid = &ilist->InterfaceInfo[i].InterfaceGuid;
					DWORD size;

					ULONG *pChNum;
					if(ERROR_SUCCESS == WlanQueryInterface (handle, guid, 
						wlan_intf_opcode_channel_number, NULL, &size, (PVOID*)&pChNum, NULL))
					{
						channel = *pChNum;
						WlanFreeMemory(pChNum);
					}

					WLAN_CONNECTION_ATTRIBUTES *conn;
					if(ERROR_SUCCESS == WlanQueryInterface (handle, guid, 
						wlan_intf_opcode_current_connection, NULL, &size, (PVOID*)&conn, NULL))
					{
						strncpy(ssid, (const char*)conn->wlanAssociationAttributes.dot11Ssid.ucSSID, DOT11_SSID_MAX_LENGTH);

						// mark as succes, even if we can't get the password
						success = true;

						DWORD dwFlags = WLAN_PROFILE_GET_PLAINTEXT_KEY;
						DWORD dwGranted = 0;
						LPWSTR pProfileXML = NULL;
	
						if(ERROR_SUCCESS == WlanGetProfile(handle, guid, conn->strProfileName, NULL, &pProfileXML, &dwFlags, &dwGranted))
						{
							int len = wcslen(pProfileXML);
							char *tstr = new char[len * 2];
							wcstombs(tstr, pProfileXML, len);

							getXMLValue(tstr, "<keyMaterial>", password);
							//OutputDebugStringW(pProfileXML);

							delete []tstr;
							WlanFreeMemory(pProfileXML);
						}

						WlanFreeMemory(conn);
					}
				}

				WlanFreeMemory(ilist);
			}
			WlanCloseHandle(handle, NULL);
		}
	}

	return success;
}

const char* errorNumToStr(int num)
{
	switch(num)
	{
	case 0: return "No error";
	case WSAEINTR: return "Interrupted system call";
	case WSAEBADF: return "Bad file number";
	case WSAEACCES: return "Permission denied";
	case WSAEFAULT: return "Bad address";
	case WSAEINVAL: return "Invalid argument";
	case WSAEMFILE: return "Too many open sockets";
	case WSAEWOULDBLOCK: return "Operation would block";
	case WSAEINPROGRESS: return "Operation now in progress";
	case WSAEALREADY: return "Operation already in progress";
	case WSAENOTSOCK: return "Socket operation on non-socket";
	case WSAEDESTADDRREQ: return "Destination address required";
	case WSAEMSGSIZE: return "Message too long";
	case WSAEPROTOTYPE: return "Protocol wrong type for socket";
	case WSAENOPROTOOPT: return "Bad protocol option";
	case WSAEPROTONOSUPPORT: return "Protocol not supported";
	case WSAESOCKTNOSUPPORT: return "Socket type not supported";
	case WSAEOPNOTSUPP: return "Operation not supported on socket";
	case WSAEPFNOSUPPORT: return "Protocol family not supported";
	case WSAEAFNOSUPPORT: return "Address family not supported";
	case WSAEADDRINUSE: return "Address already in use";
	case WSAEADDRNOTAVAIL: return "Can't assign requested address";
	case WSAENETDOWN: return "Network is down";
	case WSAENETUNREACH: return "Network is unreachable";
	case WSAENETRESET: return "Net connection reset";
	case WSAECONNABORTED: return "Software caused connection abort";
	case WSAECONNRESET: return "Connection reset by peer";
	case WSAENOBUFS: return "No buffer space available";
	case WSAEISCONN: return "Socket is already connected";
	case WSAENOTCONN: return "Socket is not connected";
	case WSAESHUTDOWN: return "Can't send after socket shutdown";
	case WSAETOOMANYREFS: return "Too many references, can't splice";
	case WSAETIMEDOUT: return "Connection timed out";
	case WSAECONNREFUSED: return "Connection refused";
	case WSAELOOP: return "Too many levels of symbolic links";
	case WSAENAMETOOLONG: return "File name too long";
	case WSAEHOSTDOWN: return "Host is down";
	case WSAEHOSTUNREACH: return "No route to host";
	case WSAENOTEMPTY: return "Directory not empty";
	case WSAEPROCLIM: return "Too many processes";
	case WSAEUSERS: return "Too many users";
	case WSAEDQUOT: return "Disc quota exceeded";
	case WSAESTALE: return "Stale NFS file handle";
	case WSAEREMOTE: return "Too many levels of remote in path";
	case WSASYSNOTREADY: return "Network system is unavailable";
	case WSAVERNOTSUPPORTED: return "Winsock version out of range";
	case WSANOTINITIALISED: return "WSAStartup not yet called";
	case WSAEDISCON: return "Graceful shutdown in progress";
	case WSAHOST_NOT_FOUND: return "Host not found";
	case WSANO_DATA: return "No host data of that type was found";
	}

	return "unknown error";
}

const char* getLastErrorMessage(int errNum = 0)
{
	static char tstr[256];

	if(errNum == 0)
		errNum = WSAGetLastError();
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

bool waitOnSocketConnect(SOCKET soc, int timeout_ms)
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
int waitOnSocketReciev(SOCKET soc, int timeout_ms)
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
	: m_soc(INVALID_SOCKET)
{
	m_isInit = false;

	// Initialize Winsock
	WSADATA k_Data;
	int iResult = WSAStartup(MAKEWORD(2,2), &k_Data);
	if(iResult == 0) 
	{
		m_isInit = true;
	}
	else
		debugPrint(DBG_WARN, "WSAStartup failed with error: %s", getLastErrorMessage(iResult));
}

Socket::~Socket()
{
	if(m_isInit)
	{
		closeStream();
		WSACleanup();
	}

	m_isInit = false;
}

bool Socket::openSocket(const char *ip, int port) 
{
	bool success = false;
	int iResult;

	if(m_isInit)
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
				m_soc = socket(pInfo->ai_family, pInfo->ai_socktype, pInfo->ai_protocol);
				if(IS_VALID(m_soc)) 
				{
#ifdef NON_BLOCKING
					// turn on non blocking mode
					unsigned long arg = 1;
					ioctlsocket(m_soc, FIONBIO, &arg);

					// Connect to server.
					connect(m_soc, pInfo->ai_addr, (int)pInfo->ai_addrlen);

					if(waitOnSocketConnect(m_soc, 5000))
						break;
#else

					// Connect to server.
					iResult = connect(m_soc, pInfo->ai_addr, (int)pInfo->ai_addrlen);
					if(iResult != SOCKET_ERROR) 
						break; // success we found a connection
					else
						debugPrint(DBG_WARN, "connect failed with error: %s", getLastErrorMessage());
#endif

					closeStream();
				}
				else
					debugPrint(DBG_WARN, "socket failed with error: %s", getLastErrorMessage());
			}
			freeaddrinfo(adrInf);

			if(IS_VALID(m_soc)) 
			{
				Sleep(500); // spin for a bit
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
	if(isOpen()) 
	{
		// tell server we are exiting
		if(shutdown(m_soc, SD_SEND) != SOCKET_ERROR) 
		{
			// wait for close signal, and drain connection
			char tbuf[1024];
			while(true)
			{
				int bytes = recv(m_soc, tbuf, 1024, 0);
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
			if(closesocket(m_soc) != SOCKET_ERROR)
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

	m_soc = INVALID_SOCKET;
}

bool Socket::isOpen() 
{ 
	return m_isInit && IS_VALID(m_soc); 
}

void Socket::clear() // is this ever needed?
{
	// call parrent
	Stream::clear();

	if(isOpen())
	{
#ifdef NON_BLOCKING
		// check if we have data waiting, without stalling
		if(1 == waitOnSocketReciev(m_soc, 50))
		{
			// log any leftover data
			const int len = 4096;
			char buf[len];
			if(read(buf, len))
				debugPrint(DBG_REPORT, "Socket::clear() leftover data: %s", buf);
		}
#else
		//****FixMe, how do we do this?
#endif
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
#ifdef NON_BLOCKING
			if(1 == waitOnSocketReciev(m_soc, 50))
#endif
			{
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
						debugPrint(DBG_LOG, "Connection closed by peer");
				}
				else
					debugPrint(DBG_WARN, "read failed with error: %s", getLastErrorMessage());
			}
#ifdef NON_BLOCKING
			//else
			//	debugPrint(DBG_WARN, "read timeout");
#endif
		}
		else
			debugPrint(DBG_LOG, "Not connected to server!");
	}

	return bytesRead;
}

int Socket::write(const char *buf, const int len)
{
	int bytesWritten = 0;

	if(isOpen() && buf && len > 0) 
	{
		int tLen = send(m_soc, buf, len, 0);
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
