#ifndef XYZ_PRINTER_LIST_H
#define XYZ_PRINTER_LIST_H

/*
Store and retrieve network information about XYZ printers in the
windows registry so we can find them later.
*/

struct WifiEntry
{
	WifiEntry()
	{
		reset();
	}

	void reset();
	void set(const char *serialNum, const char *ip);

	const static int m_len = 64;

	char m_serialNum[m_len];
	char m_ip[m_len];
	int m_port;
};

class WifiList
{
public:
	WifiList()
	{
		//m_lastPrint = -1;
		m_count = 0;
	}

	// read list form registry
	void readWifiList();

	// write list to registry
	void writeWifiList();

	WifiEntry* findEntry(const char *serialNum, bool addIfNotFound = false);

	const static int m_max = 64;

	//int m_lastPrint;
	int m_count;
	WifiEntry m_list[m_max];

protected:
	// fetch info on the last wifi printer XYZWare printed to if any
	bool readXYZLastPrint(WifiEntry *ent);
};

#endif // XYZ_PRINTER_LIST_H