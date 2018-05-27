#ifndef SERIAL_H
#define SERIAL_H

#include "stream.h"
/*
Serial class that hides win32 serial api in a nicely 
wrapped package.
*/

#ifndef HANDLE
#define HANDLE void*
#endif


class Serial : public Stream
{
public:
	Serial(); 
	~Serial();

	bool verifyPort(int port);
	bool openSerial(int port, int baudRate);
	void closeSerial();

	int getPort(); // return current port or -1 if not connected
	int getBaudRate(); // return current baudRate or -1 if not connected

	bool isOpen() { return m_serial != NULL; }
	void clear();
	int read(char *buf, int len);
	int write(const char *buf, int len);

	// from base class
	//int readLine(char *buf, int bufLen);
	//int writeStr(const char *buf);
	//int writePrintf(const char *fmt, ...);

protected:
	HANDLE m_serial;
	int m_port;
	int m_baudRate;

	static const int m_max_serial_buf = 256;
};

class SerialHelper
{
public:
	// static helper functions
	// enumerate all ports and return a default port number or -1 if no ports available
	// Pass in hint string to help guide us to best port
	static int queryForPorts(const char *hint = NULL);

	static int getPortCount() { return portCount; }
	static int getPortNumber(int id) { return (id >= 0 && id < portCount) ? portNumbers[id] : -1; }
	static const char* getPortName(int id) { return (id >= 0 && id < portCount) ? portNames[id] : NULL; }

protected:
	const static int maxPortName = 512;
	const static int maxPortCount = 24;

	static int portCount;
	static int defaultPortNum;
	static int defaultPortID;

	static int portNumbers[maxPortCount];
	static char portNames[maxPortCount][maxPortName];
};

#endif //SERIAL_H
