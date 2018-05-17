#ifndef NETWORK_H
#define NETWORK_H

#include <Winsock2.h>
#include "stream.h"

// detect wifi network windows is using
bool autoDetectWifi(char *ssid, char *password, int &channel);

class Socket : public Stream
{
public:
	Socket();
	~Socket();

	bool openSocket(const char *ip, int port);
	bool closeSocket();

	bool isOpen() { return isInit && soc != INVALID_SOCKET; }
	void clear() {} // is this ever needed?
	int read(char *buf, int bufLen);
	int write(const char *buf, int bufLen);

	// from base class
	//int readLine(char *buf, int bufLen);
	//int writeStr(const char *buf);
	//int writePrintf(const char *fmt, ...);

protected:
	WSADATA wsaData;
	SOCKET soc;
	bool isInit;
};

#endif //NETWORK_H