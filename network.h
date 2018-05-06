#ifndef NETWORK_H
#define NETWORK_H

#include <Winsock2.h>

// detect wifi network windows is using
bool autoDetectWifi(char *ssid, char *password, int &channel);

class Socket
{
public:
	Socket();
	~Socket();

	bool openSocket(const char *ip, int port);
	bool closeSocket();

	bool isOpen() { return isInit && soc != INVALID_SOCKET; }

	void clearSocket() {} // is this ever needed?

	int readSocket(char *buf, const int len);

	bool writeSocket(const char *buf);
	bool writeSocketPrintf(const char *fmt, ...);
	bool writeSocketArray(const char *buf, int len);

protected:
	WSADATA wsaData;
	SOCKET soc;
	bool isInit;
};

#endif //NETWORK_H