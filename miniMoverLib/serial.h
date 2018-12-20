#ifndef SERIAL_H
#define SERIAL_H

#include "stream.h"

//Serial class that hides win32 serial api in a nicely wrapped package.

const static int SERIAL_MAX_DEV_NAME_LEN = 256;

//-----------------

class SerialHelper
{
public:
	// static helper functions
	// enumerate all ports and return a default port number or -1 if no ports available
	// Pass in hint string to help guide us to best port
	static int queryForPorts(const char *hint = NULL);

	static int getPortCount() { return m_portCount; }
	static const char* getPortDeviceName(int id) { return (id >= 0 && id < m_portCount) ? portInfo[id].deviceName : NULL; }
	static const char* getPortDisplayName(int id) { return (id >= 0 && id < m_portCount) ? portInfo[id].displayName : NULL; }

protected:
	const static int m_maxPortCount = 24;


	static int m_portCount;
	static int m_defaultPortID;

	struct PortInfo
	{
		// string used to open port for read/write
		char deviceName[SERIAL_MAX_DEV_NAME_LEN];
		// user friendly string
		char displayName[SERIAL_MAX_DEV_NAME_LEN];
	};

	static PortInfo portInfo[m_maxPortCount];
};

//-----------------

class Serial : public StreamT
{
public:
	Serial(); 
	~Serial();

	bool openStream(const char *deviceName, int baudRate);

	virtual bool reopenStream();
	virtual void closeStream();
	virtual bool isOpen();
	virtual void clear();
	virtual int read(char *buf, int len, bool zeroTerminate = true);
	virtual int write(const char *buf, int len);

	virtual bool isWIFI() { return false; }

	virtual float getDefaultTimeout() { return 5.0f; }

	// from base class
	//int readLine(char *buf, int bufLen, bool readPartial = false);
	//int writeStr(const char *buf);
	//int writePrintf(const char *fmt, ...);

protected:

#ifdef _WIN32
	HANDLE m_handle;
#else
	int m_handle;
#endif

	char m_deviceName[SERIAL_MAX_DEV_NAME_LEN];
	int m_baudRate;

	static const int m_max_serial_recieve_buf = 1024;
};

//-----------------

#endif //SERIAL_H
