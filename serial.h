#ifndef SERIAL_H
#define SERIAL_H

class Serial
{
public:
	Serial() 
		: m_serial(NULL) 
		, m_port(-1)
		, m_baudRate(-1)
		, serBufStart(serBuf)
		, serBufEnd(serBuf)
	{}
	~Serial() { closePort(); }

	bool verifyPort(int port);
	bool openPort(int port, int baudRate);
	void closePort();

	int getPort(); // return current port or -1 if not connected
	int getBaudRate(); // return current baudRate or -1 if not connected

	bool isOpen() { return m_serial != NULL; }

	void clearSerial();
	//bool serialHasData();

	// read a string
	int readSerial(char *buf, int len);
	// read to first newline, eating the newline character
	int readSerialLine(char *buf, int len);

	// write a string
	int writeSerial(const char *buf);
	int writeSerialPrintf(const char *fmt, ...);
	int writeSerialArray(const char *buf, int len);

	// write a byte
	int writeSerialByteU8(unsigned char num);
	// write high and low bytes of short
	int writeSerialByteU16(unsigned short num, bool isLittle);
	// write 4 bytes of int
	int writeSerialByteU32(unsigned int num, bool isLittle);

	static const int m_max_serial_buf = 256;

protected:
	HANDLE m_serial;
	int m_port;
	int m_baudRate;

	// readSerialLine data
	const static int serBufLen = 4096;
	char serBuf[serBufLen];
	char* serBufStart;
	char* serBufEnd;

	// static helper functions
public:
	// enumerate all ports and return a default port number or -1 if no ports available
	// Pass in hint string to help guide us to best port
	static int queryForPorts(const char *hint = NULL);

	static int getPortCount() { return portCount; }
	static int getPortNumber(int id) { return (id >= 0 && id < portCount) ? portNumbers[id] : NULL; }
	static const char* getPortName(int id) { return (id >= 0 && id < portCount) ? portNames[id] : NULL; }

	// set to true to dump debug info to console
	void setVerbose(bool verbose) { isVerbose = verbose; }

protected:
	const static int maxPortName = 512;
	const static int maxPortCount = 24;

	static int portCount;
	static int defaultPortNum;

	static int portNumbers[maxPortCount];
	static char portNames[maxPortCount][maxPortName];

	static bool isVerbose;
	void debugPrint(const char *format, ...);
};

#endif //SERIAL_H
