#ifndef NETWORK_H
#define NETWORK_H

#ifdef _WIN32
# include <Winsock2.h>
#endif

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

	bool isOpen();
	void clear();
	int read(char *buf, int bufLen);
	int write(const char *buf, int bufLen);

	// from base class
	//int readLine(char *buf, int bufLen);
	//int writeStr(const char *buf);
	//int writePrintf(const char *fmt, ...);

protected:

#ifdef _WIN32
	WSADATA wsaData;
	SOCKET soc;
#else
	int soc;
#endif

	bool isInit;
};

#endif //NETWORK_H