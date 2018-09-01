#ifndef NETWORK_H
#define NETWORK_H

#ifdef _WIN32
# include <Winsock2.h>
#endif

// detect wifi network windows is using
bool autoDetectWifi(char *ssid, char *password, int &channel);

class Socket : public Stream
{
public:
	Socket();
	~Socket();

	bool openSocket(const char *ip, int port);

	void closeStream();
	bool isOpen();
	void clear();
	int read(char *buf, int len);
	int write(const char *buf, int len);

	bool isWIFI() { return true; }


	// from base class
	//int readLine(char *buf, int len);
	//int writeStr(const char *buf);
	//int writePrintf(const char *fmt, ...);

	float getDefaultTimeout() { return 20.0f; }

protected:
	void setSocetTimeout(int readTimeout_ms, int writeTimeout_ms);
	int waitOnSocket(int timeout_ms, bool checkWrite);

#ifdef _WIN32
	bool m_isInit;
	SOCKET m_soc;
#else
	int m_soc;
#endif

};

#endif //NETWORK_H