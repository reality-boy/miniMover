#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

#include <Wlanapi.h>
#pragma comment(lib, "Wlanapi.lib")

#include <Winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "debug.h"
#include "network.h"

#pragma warning(disable:4996)

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
		closeSocket();
		WSACleanup();
	}

	isInit = false;
}

bool Socket::openSocket(const char *ip, int port) 
{
	bool success = false;
	int iResult;

	if(isInit)
	{
		addrinfo hints;
		ZeroMemory(&hints, sizeof(hints));
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
			addrinfo *pInfo = NULL;
			for(addrinfo *pInfo = adrInf; pInfo != NULL; pInfo = pInfo->ai_next) 
			{
				// Create a SOCKET for connecting to server
				soc = socket(pInfo->ai_family, pInfo->ai_socktype, pInfo->ai_protocol);
				if(soc != INVALID_SOCKET) 
				{
					// Connect to server.
					//****FixMe, deal with blocking?
					iResult = connect(soc, pInfo->ai_addr, (int)pInfo->ai_addrlen);
					if(iResult != SOCKET_ERROR) 
					{
						break; // success we found a connection
					}

					closesocket(soc);
					soc = INVALID_SOCKET;
				}
				else
					debugPrint(DBG_WARN, "socket failed with error: %ld\n", WSAGetLastError());
			}
			freeaddrinfo(adrInf);

			if(soc != INVALID_SOCKET) 
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

	if(isInit && soc != INVALID_SOCKET) 
	{
		closesocket(soc);
		success = true;
	}
	else
		debugPrint(DBG_WARN, "Not connected to server!\n");

	return success;
}

int Socket::write(const char *buf, const int bufLen)
{
	int bytesWritten = 0;

	if(isInit && soc != INVALID_SOCKET) 
	{
		// Send an initial buffer
		//****FixMe, do we need to loop to send it all?
		//****FixMe, deal with blocking
		bytesWritten = send(soc, buf, bufLen, 0);
		if(bytesWritten != SOCKET_ERROR)
		{
			debugPrint(DBG_LOG, "Bytes Sent: %ld\n", bytesWritten);

			//if(bytesWritten == strlen(buf))
		}
		else
			debugPrint(DBG_WARN, "send failed with error: %d\n", WSAGetLastError());
	}
	else
		debugPrint(DBG_WARN, "Not connected to server!\n");

	return bytesWritten;
}

int Socket::read(char *buf, int bufLen)
{
	int count = -1;

	if(buf)
	{
		*buf = '\0';

		if(isInit && soc != INVALID_SOCKET) 
		{
			int iResult = -1;
			//****FixMe, deal with blocking
			iResult = recv(soc, buf, bufLen, 0);
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

/*
const static int dataLen = 512;
char data[dataLen];
Socket socket;
if(socket.openSocket("216.58.216.14", 80))
{
	if(socket.writeSocket("GET\n\n")) 
	{
		int count;
		while(count = socket.read(data, dataLen) > 0)
		{
			// do something with the data
			count = count;
		}
	}
	socket.closeSocket();
}
*/
