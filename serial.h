#ifndef SERIAL_H
#define SERIAL_H

class Serial
{
public:
	Serial() 
		: m_serial(NULL) 
		, m_port(-1)
		, m_baudRate(-1)
	{}
	~Serial() { closePort(); }

	static int enumeratePorts(int list[], int *count);

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
	int writeSerialByteU16(unsigned short num);
	// write 4 bytes of int
	int writeSerialByteU32(unsigned int num);

	static const int m_max_serial_buf = 256;

protected:
	HANDLE m_serial;
	int m_port;
	int m_baudRate;
};

#endif //SERIAL_H
