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

//FormatMessage() 
Socket::Socket()
	: soc(INVALID_SOCKET)
{
	isInit = false;

	// Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(iResult == 0) 
	{
		isInit = true;
	}
	else
		debugPrint(DBG_WARN, "WSAStartup failed with error: %d\n", iResult);
}

Socket::~Socket()
{
	if(isInit)
	{
		Socket::closeSocket();
		WSACleanup();
	}

	isInit = false;
}

void Socket::setSocetTimeout(int readTimeout_ms, int writeTimeout_ms)
{
	timeval timeout = {0};

	if(IS_VALID(soc))
	{
		timeout.tv_usec = readTimeout_ms * 1000;
		setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

		timeout.tv_usec = writeTimeout_ms * 1000;
		setsockopt(soc, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
	}
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
					setSocetTimeout(50, 5000);

					// Connect to server.
					iResult = connect(soc, pInfo->ai_addr, (int)pInfo->ai_addrlen);
					if(iResult != SOCKET_ERROR) 
					{
						break; // success we found a connection
					}

					Socket::closeSocket();
				}
				else
					debugPrint(DBG_WARN, "socket failed with error: %ld\n", WSAGetLastError());
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
		//shutdown(soc, SD_BOTH);
		closesocket(soc);

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
			iResult = recv(soc, buf, len, 0);
			if(iResult >= 0)
			{
				debugPrint(DBG_LOG, "Bytes received: %d\n", iResult);
				count = iResult;
			}
			else
				debugPrint(DBG_WARN, "recv failed with error: %d\n", WSAGetLastError());
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
		bytesWritten = send(soc, buf, len, 0);
		if(bytesWritten != SOCKET_ERROR)
		{
			debugPrint(DBG_LOG, "write array: %d bytes\n", bytesWritten);
			debugPrintArray(DBG_VERBOSE, buf, len);

			//if(bytesWritten == strlen(buf))
		}
		else
			debugPrint(DBG_WARN, "failed to write bytes: %d\n", WSAGetLastError());
	}
	else
		debugPrint(DBG_WARN, "Not connected to server!\n");

	return bytesWritten;
}

