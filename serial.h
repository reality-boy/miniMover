#ifndef SERIAL_H
#define SERIAL_H

/*
Serial class that hides win32 serial api in a nicely 
wrapped package.
*/

class Serial
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

	void clearSerial();

	int readSerial(char *buf, int len);
	// read to first newline, eating the newline character
	int readSerialLine(char *buf, int len);

	int writeSerial(const char *buf);
	int writeSerialPrintf(const char *fmt, ...);
	int writeSerialArray(const char *buf, int len);

protected:
	HANDLE m_serial;
	int m_port;
	int m_baudRate;

	// readSerialLine data
	const static int serBufLen = 4096;
	char serBuf[serBufLen];
	char* serBufStart;
	char* serBufEnd;

	static const int m_max_serial_buf = 256;

public:
	// static helper functions
	// enumerate all ports and return a default port number or -1 if no ports available
	// Pass in hint string to help guide us to best port
	static int queryForPorts(const char *hint = NULL);

	static int getPortCount() { return portCount; }
	static int getPortNumber(int id) { return (id >= 0 && id < portCount) ? portNumbers[id] : NULL; }
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
