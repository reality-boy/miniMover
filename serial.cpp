#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
# include <SDKDDKVer.h>
# include <Windows.h>
# include <Setupapi.h>
# include <winioctl.h>
# pragma comment(lib, "Setupapi.lib")
# pragma warning(disable:4996)
#else
#include <assert.h>
assert(false); // link to serial_linux.cpp for now!!!
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

#include "debug.h"
#include "serial.h"

//------------------------

int SerialHelper::portCount = 0;
int SerialHelper::defaultPortID = -1;
SerialHelper::PortInfo SerialHelper::portInfo[SerialHelper::maxPortCount] = {0};

// enumerate all available ports and find there string name as well
// only usb serial ports have a string name, but that is most serial devices these days
// based on CEnumerateSerial http://www.naughter.com/enumser.html
int SerialHelper::queryForPorts(const char *hint)
{
	portCount = 0;
	defaultPortID = -1;
	bool hintFound = false;

	HDEVINFO hDevInfoSet = SetupDiGetClassDevsA( &GUID_DEVINTERFACE_COMPORT, 
								NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if(hDevInfoSet != INVALID_HANDLE_VALUE)
	{
		SP_DEVINFO_DATA devInfo;
		devInfo.cbSize = sizeof(SP_DEVINFO_DATA);

		const static int maxLen = 512;
		char tstr[maxLen];

		int nIndex = 0;
		while(SetupDiEnumDeviceInfo(hDevInfoSet, nIndex, &devInfo))
		{
			int nPort = -1;
			HKEY key = SetupDiOpenDevRegKey(hDevInfoSet, &devInfo, DICS_FLAG_GLOBAL, 
											0, DIREG_DEV, KEY_QUERY_VALUE);
			if(key != INVALID_HANDLE_VALUE)
			{
				tstr[0] = '\0';
				ULONG tLen = maxLen;
				DWORD dwType = 0;
				LONG status = RegQueryValueExA(key, "PortName", nullptr, &dwType, reinterpret_cast<LPBYTE>(tstr), &tLen);
				if(ERROR_SUCCESS == status && tLen > 0 && tLen < maxLen)
				{
					// it may be possible for string to be unterminated
					// add an extra terminator just to be safe
					tstr[tLen] = '\0';

					// check for COMxx wher xx is a number
					if (strlen(tstr) > 3 && strncmp(tstr, "COM", 3) == 0 && isdigit(tstr[3]))
					{
						// srip off the number
						nPort = atoi(tstr+3);
						if (nPort != -1)
						{
							// if this triggers we need to increase maxPortCount
							assert(portCount < maxPortCount);
							if(portCount < maxPortCount)
							{
								sprintf(portInfo[portCount].deviceName, "\\\\.\\COM%d", nPort);

								// pick highest port by default, unless already found by hint
								if(!hintFound)
								{
									defaultPortID = portCount;
								}

								DWORD dwType = 0;
								DWORD dwSize = SERIAL_MAX_DEV_NAME_LEN;
								portInfo[portCount].displayName[0] = '\0';
								if(SetupDiGetDeviceRegistryPropertyA(hDevInfoSet, &devInfo, SPDRP_DEVICEDESC, &dwType, (PBYTE)portInfo[portCount].displayName, SERIAL_MAX_DEV_NAME_LEN, &dwSize))
								{
									// append device name to display name
									int len = strlen(portInfo[portCount].displayName);
									sprintf(portInfo[portCount].displayName + len, " (%s)", portInfo[portCount].deviceName);

									// check if this port matches our hint
									// for now we take the last match
									if(hint && strstr(portInfo[portCount].displayName, hint))
									{
										hintFound = true;
										defaultPortID = portCount;
									}
								}
								else
									sprintf(portInfo[portCount].displayName, portInfo[portCount].deviceName);

								portCount++;
							}
						}
					}
				}
				RegCloseKey(key);
			}

			++nIndex;
		}

		if(SerialHelper::getPortDisplayName(defaultPortID))
			debugPrint(DBG_LOG, "detected port: %d:%s", defaultPortID, SerialHelper::getPortDisplayName(defaultPortID));
		else
			debugPrint(DBG_LOG, "detected port: %d:NULL", defaultPortID);

		for(int i=0; i<SerialHelper::getPortCount(); i++)
			debugPrint(DBG_LOG, "  %d:%s", i, SerialHelper::getPortDisplayName(i));

		SetupDiDestroyDeviceInfoList(hDevInfoSet);
	}
	
	return defaultPortID;
}

//------------------------

Serial::Serial() 
	: m_handle(NULL) 
	, m_baudRate(-1)
{
	m_deviceName[0] = '\0';
}

Serial::~Serial() 
{ 
	closeSerial(); 
}

/*
bool setupConfig(const char *portStr, HANDLE h)
{
 	COMMCONFIG commConfig = {0};
	DWORD dwSize = sizeof(commConfig);
	commConfig.dwSize = dwSize;
	if(GetDefaultCommConfigA(portStr, &commConfig, &dwSize))
	{
		// Set the default communication configuration
		if (SetCommConfig(h, &commConfig, dwSize))
		{
			return true;
		}
	}
	return false;
}
*/

bool Serial::openSerial(const char *deviceName, int baudRate)
{
	bool blocking = false; // set to true to block till data arives
	int timeout_ms = 50;

	if(deviceName)
	{
		// if already connected just return
		if(0 == strcmp(deviceName, m_deviceName) && 
			getBaudRate() == baudRate)
			return true;

		// close out any previous connection
		closeSerial();
	
		// open serial port
		m_handle = CreateFileA( deviceName, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(m_handle != INVALID_HANDLE_VALUE)
		{
			//if(setupConfig(deviceName, m_handle))
			{
				// setup serial port baud rate
				DCB dcb = {0};
				if (GetCommState(m_handle, &dcb))
				{
					dcb.BaudRate=baudRate;
					dcb.ByteSize=8;
					dcb.StopBits=ONESTOPBIT;
					dcb.Parity=NOPARITY;
					//dcb.fParity = 0;

					if(SetCommState(m_handle, &dcb))
					{
						COMMTIMEOUTS cto;
						if(GetCommTimeouts(m_handle, &cto))
						{
							cto.ReadIntervalTimeout = (blocking) ? 0 : MAXDWORD; // wait forever or wait ReadTotalTimeoutConstant
							cto.ReadTotalTimeoutConstant = timeout_ms;
							cto.ReadTotalTimeoutMultiplier = 0;
							//cto.WriteTotalTimeoutMultiplier = 20000L / baudRate;
							//cto.WriteTotalTimeoutConstant = 0;

							if(SetCommTimeouts(m_handle, &cto))
							{
								if(SetCommMask(m_handle, EV_BREAK|EV_ERR|EV_RXCHAR))
								{
									if(SetupComm(m_handle, m_max_serial_recieve_buf, m_max_serial_recieve_buf))
									{
										clear();

										strcpy(m_deviceName, deviceName);
										m_baudRate = baudRate;

										return true;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	closeSerial();
	return false;
}

void Serial::closeSerial()
{
	if(m_handle)
		CloseHandle(m_handle);
	m_handle = NULL;
	m_baudRate = -1;
	m_deviceName[0] = '\0';
}

const char* Serial::getDeviceName() // return current connected device
{
	return (m_handle) ? m_deviceName : NULL;
}

int Serial::getBaudRate()
{
	return (m_handle) ? m_baudRate : -1;
}

bool Serial::isOpen() 
{ 
	return m_handle != NULL; 
}
 
void Serial::clear()
{
	// call parrent
	Stream::clear();

	// check if we have data waiting, without stalling
	DWORD dwErrorFlags;
	COMSTAT ComStat;
	ClearCommError(m_handle, &dwErrorFlags, &ComStat);
	if(ComStat.cbInQue)
	{
		// log any leftover data
		const int len = 4096;
		char buf[len];
		if(read(buf, len))
			debugPrint(DBG_REPORT, "leftover data: %s", buf);
	}

	//if(m_handle)
	//	PurgeComm (m_handle, PURGE_TXABORT | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_RXCLEAR);
}

int Serial::read(char *buf, int len)
{
	DWORD bytesRead = 0;

	if(m_handle && buf && len > 0)
	{
		//****FixMe, drain readLine buffer first!
		buf[0] = '\0';
		if(ReadFile(m_handle, buf, len-1, &bytesRead, NULL) && bytesRead > 0)
		{
			if(bytesRead > (DWORD)(len-1))
				bytesRead = len-1;

			buf[bytesRead] = '\0';
		}
	}

	return bytesRead;
}



int Serial::write(const char *buf, int len)
{
	DWORD bytesWritten = 0;

	if(m_handle && buf && len > 0)
	{
		if(WriteFile(m_handle, buf, len, &bytesWritten, NULL))
		{
			// success

			debugPrint(DBG_LOG, "write array: %d bytes", len);
			debugPrintArray(DBG_VERBOSE, buf, len);
		}
		else
			debugPrint(DBG_ERR, "failed to write bytes");
	}

	return bytesWritten;
}

