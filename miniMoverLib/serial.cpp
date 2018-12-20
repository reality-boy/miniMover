//****Note, windows only!
#ifndef _WIN32
# include <assert.h>
  assert(false);
#endif

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <Windows.h>
#include <Setupapi.h>
#include <winioctl.h>
#pragma comment(lib, "Setupapi.lib")
#pragma warning(disable:4996)

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

// enumerate all available ports and find there string name as well
// only usb serial ports have a string name, but that is most serial devices these days
// based on CEnumerateSerial http://www.naughter.com/enumser.html
int SerialHelper::queryForPorts(const char *hint)
{
	debugPrint(DBG_LOG, "SerialHelper::queryForPorts(%s)", hint);

	m_portCount = 0;
	m_defaultPortID = -1;
	bool hintFound = false;


	HDEVINFO hDevInfoSet = SetupDiGetClassDevsA(&GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR, NULL, NULL, DIGCF_PRESENT);
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
							// if this triggers we need to increase m_maxPortCount
							assert(m_portCount < m_maxPortCount);
							if(m_portCount < m_maxPortCount)
							{
								sprintf(portInfo[m_portCount].deviceName, "\\\\.\\COM%d", nPort);

								// pick highest port by default, unless already found by hint
								if(!hintFound)
								{
									m_defaultPortID = m_portCount;
								}

								DWORD dwType = 0;
								DWORD dwSize = SERIAL_MAX_DEV_NAME_LEN;
								portInfo[m_portCount].displayName[0] = '\0';
								if(SetupDiGetDeviceRegistryPropertyA(hDevInfoSet, &devInfo, SPDRP_DEVICEDESC, &dwType, (PBYTE)portInfo[m_portCount].displayName, SERIAL_MAX_DEV_NAME_LEN, &dwSize))
								{
									// append device name to display name
									int len = strlen(portInfo[m_portCount].displayName);

									// strip leading slashes from windows port def
									const char *dev = portInfo[m_portCount].deviceName;
									if(0 == strncmp("\\\\.\\", dev, strlen("\\\\.\\")))
										dev += strlen("\\\\.\\");

									sprintf(portInfo[m_portCount].displayName + len, " (%s)", dev);

									// check if this port matches our hint
									// for now we take the last match
									if(hint && strstr(portInfo[m_portCount].displayName, hint))
									{
										hintFound = true;
										m_defaultPortID = m_portCount;
									}
								}
								else
								{
									debugPrint(DBG_LOG, "SerialHelper::queryForPorts failed to find display name for port %s", portInfo[m_portCount].deviceName);
									sprintf(portInfo[m_portCount].displayName, portInfo[m_portCount].deviceName);
								}

								m_portCount++;
							}
							else
								debugPrint(DBG_WARN, "SerialHelper::queryForPorts m_maxPortCount exeeded %d", m_maxPortCount);
						}
					}
					else
						debugPrint(DBG_LOG, "SerialHelper::queryForPorts skipping device '%s'", tstr);
				}
				else
					debugPrint(DBG_WARN, "SerialHelper::queryForPorts RegQueryValueEx failed with error %d", status);

				RegCloseKey(key);
			}
			else
				debugPrint(DBG_WARN, "SerialHelper::queryForPorts SetupDiOpenDevRegKey failed with error %d", GetLastError());

			++nIndex;
		}


		if(m_portCount > 0)
		{
			for(int i=0; i<m_portCount; i++)
			{
				debugPrint(DBG_LOG, "SerialHelper::queryForPorts found port  %d:%s", i, SerialHelper::getPortDisplayName(i));
			}

			if(m_defaultPortID >= 0)
			{
				if(SerialHelper::getPortDisplayName(m_defaultPortID))
					debugPrint(DBG_LOG, "SerialHelper::queryForPorts default port: %d:%s", m_defaultPortID, SerialHelper::getPortDisplayName(m_defaultPortID));
				else
					debugPrint(DBG_LOG, "SerialHelper::queryForPorts default port: %d:NULL", m_defaultPortID);
			}
		}
		else
			debugPrint(DBG_LOG, "SerialHelper::queryForPorts failed to find any ports");

		SetupDiDestroyDeviceInfoList(hDevInfoSet);
	}
	else
		debugPrint(DBG_WARN, "SerialHelper::queryForPorts SetupDiGetClassDevs failed with error %d", GetLastError());
	
	return m_defaultPortID;
}

//------------------------

Serial::Serial() 
	: m_handle(INVALID_HANDLE_VALUE) 
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

/*
bool Serial::setupConfig(const char *portStr, HANDLE h)
{
	debugPrint(DBG_LOG, "Serial::setupConfig()");

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

int getInt(const char *str)
{
	while(str && *str)
	{
		if(isdigit(*str))
			return atoi(str);
		str++;
	}

	return -1;
}

bool Serial::openStream(const char *deviceName, int baudRate)
{
	debugPrint(DBG_LOG, "Serial::openStream(%s, %d)", deviceName, baudRate);

	assert(deviceName);
	assert(baudRate > 0);

	bool blocking = false; // set to true to block till data arives
	int timeout_ms = 50;

	// try to pull port number from string
	int port = getInt(deviceName);
	if(port >= 0)
	{
		// then upconvert it to full port path
		char name[SERIAL_MAX_DEV_NAME_LEN];
		sprintf(name, "\\\\.\\COM%d", port);

		// if already connected just return
		if( isOpen() &&
			0 == strcmp(name, m_deviceName) && 
			m_baudRate == baudRate )
		{
			return true;
		}

		// close out any previous connection
		if(isOpen()) 
			closeStream();
	
		// open serial port
		m_handle = CreateFileA( name, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(isOpen())
		{
			//if(setupConfig(name, m_handle))
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
							// don't set this, we want to time out quickly if no data available
							cto.ReadTotalTimeoutMultiplier = 0; 

							cto.WriteTotalTimeoutConstant = timeout_ms;
							// when sending lots of data we want to extend the timeout
							cto.WriteTotalTimeoutMultiplier = 2 * baudRate / (11 * 1000);

							if(SetCommTimeouts(m_handle, &cto))
							{
								if(SetCommMask(m_handle, EV_BREAK|EV_ERR|EV_RXCHAR))
								{
									if(SetupComm(m_handle, m_max_serial_recieve_buf, m_max_serial_recieve_buf))
									{
										clear();

										strcpy(m_deviceName, name);
										m_baudRate = baudRate;

										debugPrint(DBG_LOG, "Serial::openStream connected to %s : %d", m_deviceName, m_baudRate);

										return true;
									}
									else
										debugPrint(DBG_WARN, "Serial::openStream SetupComm failed with error %d", GetLastError());
								}
								else
									debugPrint(DBG_WARN, "Serial::openStream SetCommMask failed with error %d", GetLastError());
							}
							else
								debugPrint(DBG_WARN, "Serial::openStream SetCommTimeouts failed with error %d", GetLastError());
						}
						else
							debugPrint(DBG_WARN, "Serial::openStream GetCommTimeouts failed with error %d", GetLastError());
					}
					else
						debugPrint(DBG_WARN, "Serial::openStream SetCommState failed with error %d", GetLastError());
				}
				else
					debugPrint(DBG_WARN, "Serial::openStream GetCommState failed with error %d", GetLastError());
			}
		}
		else
			debugPrint(DBG_WARN, "Serial::openStream CreateFile failed with error %d", GetLastError());
	}
	else
		debugPrint(DBG_WARN, "Serial::openStream failed invalid input");

	// Close if already open
	closeStream();

	return false;
}

bool Serial::reopenStream()
{
	debugPrint(DBG_LOG, "Serial::reopenStream()");

	if(isOpen()) 
		closeStream();

	if(*m_deviceName)
		return openStream(m_deviceName, m_baudRate);

	return false;
}

void Serial::closeStream()
{
	debugPrint(DBG_LOG, "Serial::closeStream()");

	if(isOpen())
	{
		// drain buffers
		//StreamT::clear();

		CloseHandle(m_handle);
	}
	else
		debugPrint(DBG_VERBOSE, "Serial::closeStream failed invalid connection");

	m_handle = INVALID_HANDLE_VALUE;

	// don't forget about our last connection
	//m_baudRate = -1;
	//m_deviceName[0] = '\0';
}

bool Serial::isOpen()
{
	debugPrint(DBG_VERBOSE, "Serial::isOpen()");

	return m_handle != INVALID_HANDLE_VALUE; 
}

void Serial::clear()
{
	debugPrint(DBG_VERBOSE, "Serial::clear()");

	// call parent
	StreamT::clear();

	if(isOpen())
	{
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
				debugPrint(DBG_REPORT, "Serial::clear leftover data: '%s'", buf);
		}
	}
	else
		debugPrint(DBG_VERBOSE, "Serial::clear failed invalid connection");
}

int Serial::read(char *buf, int len, bool zeroTerminate)
{
	debugPrint(DBG_VERBOSE, "Serial::read()");

	assert(buf);
	assert(len > 0);

	if(buf && len > 0)
	{
		//if(zeroTerminate) // no harm in terminating here
		buf[0] = '\0';

		if(isOpen())
		{
			DWORD tLen;
			if(ReadFile(m_handle, buf, (zeroTerminate) ? len-1 : len, &tLen, NULL))
			{
				if(tLen > 0)
				{
					// success
					if(zeroTerminate)
						buf[tLen] = '\0';
					debugPrint(DBG_LOG, "Serial::read returned %d bytes", tLen);

					return tLen;
				}
			}
			else
			{
				debugPrint(DBG_WARN, "Serial::read failed with error %d", GetLastError());
				closeStream();
			}
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
			DWORD tLen;
			if(WriteFile(m_handle, buf, len, &tLen, NULL))
			{
				// success
				debugPrint(DBG_LOG, "Serial::write sent: %d of %d bytes", tLen, len);
				return tLen;
			}
			else
			{
				debugPrint(DBG_WARN, "Serial::write failed with error %d", GetLastError());
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

