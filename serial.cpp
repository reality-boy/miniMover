#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <Setupapi.h>
#include <assert.h>

#include "serial.h"
#include "debug.h"

#pragma comment(lib, "Setupapi.lib")
#pragma warning(disable:4996)

// static class data

int Serial::portCount = 0;
int Serial::defaultPortNum = -1;
int Serial::defaultPortID = -1;

int Serial::portNumbers[maxPortCount] = {-1};
char Serial::portNames[maxPortCount][maxPortName] = {""};

bool Serial::verifyPort(int port)
{
	char portStr[64] = "";
	sprintf(portStr, "\\\\.\\COM%d", port);

	// open serial port
	HANDLE serial = CreateFileA( portStr, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(serial != INVALID_HANDLE_VALUE)
	{
		CloseHandle(serial);
		return true;
	}

	return false;
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

bool Serial::openPort(int port, int baudRate)
{
	bool blocking = false; // set to true to block till data arives
	int timeout_ms = 50;

	// if already connected just return
	if( (port < 0 || getPort() == port) && 
		getBaudRate() == baudRate)
		return true;

	// close out any previous connection
	closePort();
	
	// auto detect port
	if(port < 0)
		port = queryForPorts();

	if(port >= 2) // reject default ports, maybe there is a better way?
	{
		char portStr[64] = "";
		sprintf(portStr, "\\\\.\\COM%d", port);

		// open serial port
		m_serial = CreateFileA( portStr, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(m_serial != INVALID_HANDLE_VALUE)
		{
			//if(setupConfig(portStr, m_serial))
			{
				// setup serial port baud rate
				DCB dcb = {0};
				if (GetCommState(m_serial, &dcb))
				{
					dcb.BaudRate=baudRate;
					dcb.ByteSize=8;
					dcb.StopBits=ONESTOPBIT;
					dcb.Parity=NOPARITY;
					//dcb.fParity = 0;

					if(SetCommState(m_serial, &dcb))
					{
						COMMTIMEOUTS cto;
						if(GetCommTimeouts(m_serial, &cto))
						{
							cto.ReadIntervalTimeout = (blocking) ? 0 : MAXDWORD; // wait forever or wait ReadTotalTimeoutConstant
							cto.ReadTotalTimeoutConstant = timeout_ms;
							cto.ReadTotalTimeoutMultiplier = 0;
							//cto.WriteTotalTimeoutMultiplier = 20000L / baudRate;
							//cto.WriteTotalTimeoutConstant = 0;

							if(SetCommTimeouts(m_serial, &cto))
							{
								if(SetCommMask(m_serial, EV_BREAK|EV_ERR|EV_RXCHAR))
								{
									if(SetupComm(m_serial, m_max_serial_buf, m_max_serial_buf))
									{
										clearSerial();

										m_port = port;
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

		closePort();
	}
	return false;
}

void Serial::closePort()
{
	if(m_serial)
		CloseHandle(m_serial);
	m_serial = NULL;
	m_port = -1;
	m_baudRate = -1;
}

int Serial::getPort()
{
	return (m_serial) ? m_port : -1;
}

int Serial::getBaudRate()
{
	return (m_serial) ? m_baudRate : -1;
}
 
void Serial::clearSerial()
{
	if(m_serial)
		PurgeComm (m_serial, PURGE_TXABORT | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_RXCLEAR);

	//****FixMe, drain buffer and log to error log

	serBuf[0] = '\0';
	serBufStart = serBuf;
	serBufEnd = serBuf;
}

int Serial::readSerial(char *buf, int len)
{
	DWORD bytesRead = 0;

	if(m_serial && buf && len > 0)
	{
		//****FixMe, drain readSerialLine buffer first!
		buf[0] = '\0';
		if(ReadFile(m_serial, buf, len-1, &bytesRead, NULL) && bytesRead > 0)
		{
			if(bytesRead > (DWORD)(len-1))
				bytesRead = len-1;

			buf[bytesRead] = '\0';
		}
	}

	return bytesRead;
}

int Serial::readSerialLine(char *lineBuf, int len)
{
	if(lineBuf && len > 0)
	{
		*lineBuf = '\0';
		char *lineBufStart = lineBuf;
		const char *lineBufEnd = &lineBuf[len-1];

		//loop around at least twicw in order to ensure we drained the buffer and found a new line
		for(int i=0; i<2; i++)
		{
			if(serBufStart == serBufEnd)
			{
				serBufStart = serBuf;
				int len = readSerial(serBuf, serBufLen);
				serBufEnd = &serBuf[len];
			}

			if(serBufStart != serBufEnd)
			{
				bool isDone = false;
				while(lineBufStart != lineBufEnd && serBufStart != serBufEnd)
				{
					*lineBufStart = *serBufStart;

					if(*lineBufStart == '\n')
					{
						*lineBufStart = '\0';
						isDone = true;
					}

					lineBufStart++;
					serBufStart++;

					if(isDone)
					{
						int len = lineBufStart - lineBuf;

						if(len > 0)
							debugPrint(DBG_LOG, "recieved: %s", lineBuf);

						return len;
					}
				}
			}
		}

		*lineBufStart = '\0';
		int len = lineBufStart - lineBuf;

		if(len > 0)
			debugPrint(DBG_LOG, "recieved partial: %s", lineBuf);

		return len;
	}

	return 0;
}

int Serial::writeSerial(const char *buf)
{
	DWORD bytesWritten = 0;

	if(m_serial && buf)
	{
		int len = strlen(buf);

		if(WriteFile(m_serial, buf, len, &bytesWritten, NULL))
		{
			// success
			debugPrint(DBG_LOG, "sent: %s", buf);
		}
		else
			debugPrint(DBG_WARN, "wirteSerial failed to write");
	}

	return bytesWritten;
}

int Serial::writeSerialPrintf(const char *fmt, ...)
{
	if(m_serial && fmt)
	{
		static const int tstrLen = 4096;
		char tstr[tstrLen];

	    va_list arglist;
	    va_start(arglist, fmt);

	    //customized operations...
	    vsnprintf_s(tstr, tstrLen, fmt, arglist);
		tstr[tstrLen-1] = '\0';

	    va_end(arglist);

		return writeSerial(tstr);
	}

	return 0;
}

int Serial::writeSerialArray(const char *buf, int len)
{
	DWORD bytesWritten = 0;

	if(m_serial && buf && len > 0)
	{
		if(WriteFile(m_serial, buf, len, &bytesWritten, NULL))
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

// write a byte
int Serial::writeSerialByteU8(unsigned char num)
{
	DWORD bytesWritten = 0;
	if(m_serial)
	{
		if(WriteFile(m_serial, &num, sizeof(num), &bytesWritten, NULL))
		{
			debugPrint(DBG_LOG, "write byte: %d", num);
		}
		else
			debugPrint(DBG_ERR, "failed to write byte");
	}

	return bytesWritten;
}

// write high and low bytes of short
int Serial::writeSerialByteU16(unsigned short num, bool isLittle)
{
	DWORD bytesWritten = 0;
	if(isLittle) {
		bytesWritten += writeSerialByteU8((num >> 0) & 0xFF);
		bytesWritten += writeSerialByteU8((num >> 8) & 0xFF);
	} else {
		bytesWritten += writeSerialByteU8((num >> 8) & 0xFF);
		bytesWritten += writeSerialByteU8((num >> 0) & 0xFF);
	}
	return bytesWritten;
}

// write 4 bytes of int
int Serial::writeSerialByteU32(unsigned int num, bool isLittle)
{
	DWORD bytesWritten = 0;
	if(isLittle) {
		bytesWritten += writeSerialByteU8((num >> 0)  & 0xFF);
		bytesWritten += writeSerialByteU8((num >> 8)  & 0xFF);
		bytesWritten += writeSerialByteU8((num >> 16) & 0xFF);
		bytesWritten += writeSerialByteU8((num >> 24) & 0xFF);
	} else {
		bytesWritten += writeSerialByteU8((num >> 24) & 0xFF);
		bytesWritten += writeSerialByteU8((num >> 16) & 0xFF);
		bytesWritten += writeSerialByteU8((num >> 8)  & 0xFF);
		bytesWritten += writeSerialByteU8((num >> 0)  & 0xFF);
	}
	return bytesWritten;
}

// enumerate all available ports and find there string name as well
// only usb serial ports have a string name, but that is most serial devices these days
// based on CEnumerateSerial http://www.naughter.com/enumser.html
int Serial::queryForPorts(const char *hint)
{
	portCount = 0;
	defaultPortNum = -1;
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
								portNumbers[portCount] = nPort;

								// pick highest port by default, unless already found by hint
								if(!hintFound && defaultPortNum < nPort)
								{
									defaultPortNum = nPort;
									defaultPortID = portCount;
								}

								DWORD dwType = 0;
								DWORD dwSize = maxPortName;
								portNames[portCount][0] = '\0';
								if(SetupDiGetDeviceRegistryPropertyA(hDevInfoSet, &devInfo, SPDRP_DEVICEDESC, &dwType, (PBYTE)portNames[portCount], maxPortName, &dwSize))
								{
									// check if this port matches our hint
									// for now we take the last match
									if(hint && strstr(portNames[portCount], hint))
									{
										hintFound = true;
										defaultPortNum = nPort;
										defaultPortID = portCount;
									}
								}
								else
									portNames[portCount][0] = '\0';

								portCount++;
							}
						}
					}
				}
				RegCloseKey(key);
			}

			++nIndex;
		}

		if(Serial::getPortName(defaultPortID))
			debugPrint(DBG_LOG, "detected port: %d:%s", defaultPortNum, Serial::getPortName(defaultPortID));
		else
			debugPrint(DBG_LOG, "detected port: %d:NULL", defaultPortNum);
		for(int i=0; i<Serial::getPortCount(); i++)
			debugPrint(DBG_LOG, "  %d:%s", Serial::getPortNumber(i), Serial::getPortName(i));

		SetupDiDestroyDeviceInfoList(hDevInfoSet);
	}
	
	return defaultPortNum;
}

//****Note, older code that is reliable but does not give you a string name for the port
/*
int Serial::enumeratePorts(int list[], int *count)
{
	//What will be the return value
	bool bSuccess = false;

	int bestGuess = -1;
	int maxList = 0;
	if(count)
	{
		maxList = *count;
		*count = 0;
	}

	//Use QueryDosDevice to look for all devices of the form COMx. Since QueryDosDevice does
	//not consitently report the required size of buffer, lets start with a reasonable buffer size
	//and go from there
	const static int nChars = 40000;
	char pszDevices[nChars];
	DWORD dwChars = QueryDosDeviceA(NULL, pszDevices, nChars);
	if (dwChars == 0)
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_INSUFFICIENT_BUFFER)
			debugPrint(DBG_ERR, "needs more room!");
	}
	else
	{
		bSuccess = true;
		size_t i=0;

		while (pszDevices[i] != '\0')
		{
			//Get the current device name
			char* pszCurrentDevice = &pszDevices[i];

			//If it looks like "COMX" then
			//add it to the array which will be returned
			size_t nLen = strlen(pszCurrentDevice);
			if (nLen > 3)
			{
				if ((0 == strncmp(pszCurrentDevice, "COM", 3)) && isdigit(pszCurrentDevice[3]))
				{
					//Work out the port number
					int nPort = atoi(&pszCurrentDevice[3]);
					if(list && count && maxList > *count)
					{
						list[*count] = nPort;
						(*count)++;
					}
					if(bestGuess < nPort)
						bestGuess = nPort;
				}
			}

			//Go to next device name
			i += (nLen + 1);
		}
	}
	return bestGuess;
}
*/
