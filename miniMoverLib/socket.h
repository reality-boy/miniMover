#ifndef NETWORK_H
#define NETWORK_H

#ifdef _WIN32
# include <Winsock2.h>
#endif

#include "stream.h"

const static int SOCKET_MAX_ADDR_LEN = 256;

// detect wifi network windows is using
bool autoDetectWifi(char *ssid, char *password, int &channel);

class Socket : public StreamT
{
public:
	Socket();
	~Socket();

	bool openStream(const char *ip, int port);

	virtual bool reopenStream();
	virtual void closeStream();
	virtual bool isOpen();
	virtual void clear();
	virtual int read(char *buf, int len, bool zeroTerminate = true);
	virtual int write(const char *buf, int len);

	virtual bool isWIFI() { return true; }

	virtual float getDefaultTimeout() { return 5.0f; }

	// from base class
	//int readLine(char *buf, int len, bool readPartial = false);
	//int writeStr(const char *buf);
	//int writePrintf(const char *fmt, ...);

protected:
	void setSocetTimeout(int readTimeout_ms, int writeTimeout_ms);
	int waitOnSocket(int timeout_ms, bool checkWrite);

	bool m_isInit;
#ifdef _WIN32
	SOCKET m_soc;
#else
	int m_soc;
#endif

	char m_ipAddr[SOCKET_MAX_ADDR_LEN];
	int m_port;
};

#endif //NETWORK_H
