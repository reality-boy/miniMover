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
#include <assert.h>

#include "debug.h"
#include "stream.h"
#include "socket.h"

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
	debugPrint(DBG_LOG, "autoDetectWifi()");

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

// 1 - success, 0 - timeout, -1 - error
int Socket::waitOnSocket(int timeout_ms, bool checkWrite)
{
	debugPrint(DBG_VERBOSE, "Socket::waitOnSocket(%d, %d)", timeout_ms, checkWrite);

	int ret = -1;
	if(isOpen())
	{
		fd_set fdread;
	    FD_ZERO(&fdread);
	    FD_SET(m_soc, &fdread);

		fd_set fdwrite;
	    FD_ZERO(&fdwrite);
	    FD_SET(m_soc, &fdwrite);

		//fd_set fderror;
	    //FD_ZERO(&fderror);
	    //FD_SET(m_soc, &fderror);

		timeval tv;
	    tv.tv_sec = timeout_ms / 1000;
	    tv.tv_usec = (timeout_ms % 1000) * 1000;

		// ret > 0 indicates an event triggered
		// ret == 0 indicates we hit the timeout
		// ret == -1 indicates error 
		ret = select(m_soc + 1, (checkWrite) ? NULL : &fdread, (checkWrite) ? &fdwrite : NULL, NULL /*&fderror*/, &tv);
		if(ret > 0)
	    {
			int iResult;
	        socklen_t len = sizeof iResult;
	        ret = getsockopt(m_soc, SOL_SOCKET, SO_ERROR, (char*)&iResult, &len);
			if(ret >= 0)
			{
				if(iResult == 0) 
				{
					debugPrint(DBG_VERBOSE, "Socket::waitOnSocket succeeded");
					return 1; // success, connected with no timeout
				}
				else // failed with error
				{
					debugPrint(DBG_WARN, "Socket::waitOnSocket failed with error: %s", getLastErrorMessage(iResult));
					closeStream();
				}
			}
			else // failed with error
			{
				debugPrint(DBG_WARN, "Socket::waitOnSocket getsockopt failed with error: %s", getLastErrorMessage());
				closeStream();
			}

			ret = -1;
	    }
		else if(ret == 0)
		{
			if(timeout_ms > 0) // if timeout is 0 were just testing for data so don't report error
				debugPrint(DBG_LOG, "Socket::waitOnSocket hit timeout, %d ms, error %s", timeout_ms, getLastErrorMessage());
		}
		else
		{
			debugPrint(DBG_WARN, "Socket::waitOnSocket failed with timeout, %d:%s", ret, getLastErrorMessage());
			closeStream();
		}
	}
	else
		debugPrint(DBG_WARN, "Socket::waitOnSocket failed invalid connection");

	return ret;
}

Socket::Socket()
	: m_soc(INVALID_SOCKET)
{
	debugPrint(DBG_LOG, "Socket::Socket()");

	m_isInit = false;
	m_ipAddr[0] = '\0';
	m_port = 0;

	// Initialize Winsock
	WSADATA k_Data;
	int iResult = WSAStartup(MAKEWORD(2,0), &k_Data);
	if(iResult == 0) 
	{
		if(LOBYTE(k_Data.wVersion) == 2 && HIBYTE(k_Data.wVersion) == 0)
		{
			m_isInit = true;
			debugPrint(DBG_LOG, "Socket::Socket succeeded");
		}
		else
			debugPrint(DBG_WARN, "Socket::Socket WSAStartup failed with wrong version: %d:%d", LOBYTE(k_Data.wVersion), HIBYTE(k_Data.wVersion));
	}
	else
		debugPrint(DBG_WARN, "Socket::Socket WSAStartup failed with error: %s", getLastErrorMessage(iResult));
}

Socket::~Socket()
{
	debugPrint(DBG_LOG, "Socket::~Socket()");

	if(m_isInit)
	{
		Socket::closeStream();
		if(SOCKET_ERROR == WSACleanup())
			debugPrint(DBG_WARN, "Socket::~Socket WSACleanup failed with error: %s", getLastErrorMessage());
	}

	m_isInit = false;
}

bool Socket::openStream(const char *ip, int port) 
{
	debugPrint(DBG_LOG, "Socket::openStream(%s, %d)", ip, port);

	assert(ip);
	assert(port >= 0);

	if(m_isInit)
	{
		// close out any previous connection
		if(isOpen()) 
			closeStream();

		// Create a SOCKET for connecting to server
		m_soc = socket(AF_INET, SOCK_STREAM, 0);
		if(IS_VALID(m_soc)) 
		{
			// turn on non blocking mode
			unsigned long arg = 1;
			ioctlsocket(m_soc, FIONBIO, &arg);

			sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = lookupAddress(ip);
			addr.sin_port = htons(port);

			// Connect the socket to the given IP and port
			if (connect(m_soc, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR
				|| WSAGetLastError() == WSAEWOULDBLOCK
				|| WSAGetLastError() == WSAEINPROGRESS )
			{
				if(waitOnSocket(1000, true) > 0)
				{
					// stash info on current connection
					strcpy(m_ipAddr, ip);
					m_port = port;

					debugPrint(DBG_LOG, "Socket::openStream succeeded");
					return true;
				}
				else
				{
					debugPrint(DBG_WARN, "Socket::openStream socket connect failed with timeout: %s", getLastErrorMessage());
					closesocket(m_soc);
					m_soc = INVALID_SOCKET;
				}
			}
			else
			{
				debugPrint(DBG_WARN, "Socket::openStream socket connect failed with error: %s", getLastErrorMessage());
				closesocket(m_soc);
				m_soc = INVALID_SOCKET;
			}
		}
		else
			debugPrint(DBG_WARN, "Socket::openStream socket failed with error: %s", getLastErrorMessage());
	}
	else
		debugPrint(DBG_WARN, "Socket::openStream winsock not initialized");

	return false;
}

bool Socket::reopenStream()
{
	debugPrint(DBG_LOG, "Socket::reopenStream()");

	if(isOpen()) 
		closeStream();

	if(*m_ipAddr)
		return openStream(m_ipAddr, m_port);

	return false;
}

void Socket::closeStream()
{
	debugPrint(DBG_LOG, "Socket::closeStream()");

	if(isOpen()) 
	{
		// drain buffers
		//StreamT::clear();

		// tell server we are exiting
		if(shutdown(m_soc, SD_BOTH) == SOCKET_ERROR) 
			debugPrint(DBG_WARN, "Socket::closeStream shutdown failed with error: %s", getLastErrorMessage());

		// actually close the socket
		if(closesocket(m_soc) != SOCKET_ERROR)
		{
			// success
			debugPrint(DBG_VERBOSE, "Socket::closeStream succeeded");
		}
		else
			debugPrint(DBG_WARN, "Socket::closeStream close socket failed with error: %s", getLastErrorMessage());
	}
	else
		debugPrint(DBG_VERBOSE, "Socket::closeStream failed invalid connection");

	m_soc = INVALID_SOCKET;
}

bool Socket::isOpen() 
{ 
	debugPrint(DBG_VERBOSE, "Socket::isOpen()");

	return m_isInit && IS_VALID(m_soc); 
}

void Socket::clear() // is this ever needed?
{
	debugPrint(DBG_VERBOSE, "Socket::clear()");

	// call parrent
	StreamT::clear();

	if(isOpen())
	{
		// check if we have data waiting, without stalling
		if(waitOnSocket(0, false) > 0)
		{
			// log any leftover data
			const int len = 4096;
			char buf[len];
			if(read(buf, len))
				debugPrint(DBG_REPORT, "Socket::clear leftover data: '%s'", buf);
		}
	}
	else
		debugPrint(DBG_VERBOSE, "Socket::clear failed invalid connection");
}

int Socket::read(char *buf, int len, bool zeroTerminate)
{
	debugPrint(DBG_VERBOSE, "Socket::read()");

	assert(buf);
	assert(len > 0);

	if(buf && len > 0)
	{
		//if(zeroTerminate) // no harm in terminating here
		buf[0] = '\0';

		if(isOpen())
		{
			// wait for data without blocking
			int ret = waitOnSocket(100, false);
			if(ret > 0)
			{
				int tLen = recv(m_soc, buf, (zeroTerminate) ? len-1 : len, 0);
				if(tLen != SOCKET_ERROR)
				{
					if(tLen > 0)
					{
						// success
						if(zeroTerminate)
							buf[tLen] = '\0';
						debugPrint(DBG_VERBOSE, "Socket::read returned %d bytes", tLen);
						return tLen;
					}
					else
					{
						debugPrint(DBG_LOG, "Socket::read Connection closed by peer");
						closeStream();
					}
				}
				else
				{
					debugPrint(DBG_WARN, "Socket::read read failed with error: %s", getLastErrorMessage());
					closeStream();
				}
			}
			else if(0 == ret)
				debugPrint(DBG_VERBOSE, "Socket::read read timeout");
			else // -1 == ret
			{
				debugPrint(DBG_WARN, "Socket::read read errored out: %s", getLastErrorMessage());
				//closeStream(); //already closed in waitOnSocket()
			}
		}
		else
			debugPrint(DBG_WARN, "Socket::read failed invalid connection");
	}

	return 0;
}

int Socket::write(const char *buf, int len)
{
	debugPrint(DBG_VERBOSE, "Socket::write(%s, %d)", buf, len);

	assert(buf);
	assert(len > 0);

	int retLen = 0;

	if(buf && len > 0)
	{
		if(isOpen())
		{
			for(int i=0; i<10 && len > 0; i++)
			{
				int ret = waitOnSocket(100, true);
				if(ret > 0)
				{
					int tLen = send(m_soc, buf, len, 0);
					if(tLen != SOCKET_ERROR && tLen > 0)
					{
						// success
						debugPrint(DBG_LOG, "Socket::write sent: %d of %d bytes", tLen, len);
						len -= tLen;
						retLen += tLen;
					}
					else
					{
						debugPrint(DBG_WARN, "Socket::write failed with error: %s", getLastErrorMessage());
						closeStream();
						break;
					}
				}
				else if(0 == ret)
					debugPrint(DBG_VERBOSE, "Socket::write write timeout");
				else // -1 == ret
				{
					debugPrint(DBG_WARN, "Socket::write write errored out: %s", getLastErrorMessage());
					//closeStream(); //already closed in waitOnSocket()
					break;
				}
			}
		}
		else
			debugPrint(DBG_WARN, "Socket::write failed invalid connection");
	}
	else
		debugPrint(DBG_WARN, "Socket::write failed invalid input");

	return retLen;
}
