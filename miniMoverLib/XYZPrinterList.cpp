#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
# include <SDKDDKVer.h>
# include <Windows.h>
# pragma warning(disable: 4996) // disable deprecated warning 
#else
//****FixMe linux stuff
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "XYZPrinterList.h"

//****FixMe, this is very windows centric using the registry
// rewrite so we can optionally use an ini file
//****FixMe, this is not picky enough, should work harder to
// clean out duplicates
//****FixMe, keep track of last printer connected to 

void WifiEntry::reset()
{
	m_serialNum[0] = '\0';
	m_ip[0] = '\0';
	m_port = 0;
}

void WifiEntry::set(const char *serialNum, const char *ip)
{
	if(serialNum && ip)
	{
		strcpy(m_serialNum, serialNum);
		strcpy(m_ip, ip);
		m_port = 9100;
	}
	else
		reset();
}

bool WifiList::readXYZLastPrint(WifiEntry *ent)
{
	bool success = false;

	if(ent)
	{
#ifdef _WIN32
		HKEY key;
		if(RegOpenKeyA(HKEY_CURRENT_USER, "Software\\XYZware\\xyzsetting\\", &key) == ERROR_SUCCESS)
		{

			// if the default ip key is set, and is valid, 
			// then we have enough info to hook to a wifi printer
			DWORD len = WifiEntry::m_len;
			if(RegQueryValueExA(key, "DefaultIP", NULL, NULL, (LPBYTE)ent->m_ip, &len) == ERROR_SUCCESS)
			{
				// rudamentary sanity check
				if(strlen(ent->m_ip) > 2)
				{
					// machine serial number
					len = WifiEntry::m_len;
					if(RegQueryValueExA(key, "LastPrinterSN", NULL, NULL, (LPBYTE)ent->m_serialNum, &len) == ERROR_SUCCESS)
					{
						ent->m_port = 9100;
						success = true;
					}
				}
			}

			RegCloseKey(key);
		}

		// zero out strings if not found
		if(!success)
		{
			ent->m_ip[0] = '\0';
			ent->m_port = 0;
			ent->m_serialNum[0] = '\0';
		}
#endif
	}

	return success;
}

void WifiList::readWifiList()
{
#ifdef _WIN32
	m_count = 0;
	//m_lastPrint = -1;

	// open base key
	HKEY baseKey;
	if(RegOpenKeyA(HKEY_CURRENT_USER, "Software\\miniMover\\", &baseKey) == ERROR_SUCCESS)
	{
		// get index of last printer we connected to
		DWORD len = 32;
		//char tstr[32];
		//if(RegQueryValueExA(baseKey, "lastPrinterIndex", NULL, NULL, (LPBYTE)tstr, &len) == ERROR_SUCCESS)
		{
			//m_lastPrint = atoi(tstr);
			// open wifi subkey
			HKEY wifiKey;
			if(RegOpenKeyA(HKEY_CURRENT_USER, "Software\\miniMover\\wifi\\", &wifiKey) == ERROR_SUCCESS)
			{
				// enumerate subkeys
				bool found = false;
				DWORD index = 0;
				len = WifiEntry::m_len;
				while((m_count+1) < m_max &&
					RegEnumKeyExA(wifiKey, index, m_list[m_count].m_serialNum, &len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
				{
					char tpath[128];
					sprintf(tpath, "Software\\miniMover\\wifi\\%s\\", m_list[m_count].m_serialNum);
					HKEY entryKey;
					if(RegOpenKeyA(HKEY_CURRENT_USER, tpath, &entryKey) == ERROR_SUCCESS)
					{
						len = WifiEntry::m_len;
						if(RegQueryValueExA(entryKey, "ip", NULL, NULL, (LPBYTE)m_list[m_count].m_ip, &len) == ERROR_SUCCESS && len > 0)
						{
							m_list[m_count].m_port = 9100;
							found = true;
							m_count++;
						}

						RegCloseKey(entryKey);
					}

					if(!found)
						m_list[m_count].reset();

					found = false;
					len = WifiEntry::m_len;
					index++;
				}

				RegCloseKey(wifiKey);
			}
		}
		RegCloseKey(baseKey);
	}

	// append last printer from XYZWare, if not in list already
	WifiEntry ent;
	if(readXYZLastPrint(&ent))
	{
		// add it if a new printer
		if(!findEntry(ent.m_serialNum) && (m_count+1) < m_max)
		{
			m_list[m_count].set(ent.m_serialNum, ent.m_ip);
			m_count++;
		}
	}

	// if last print does not point to a valid entry, set it to unknown
	//if(m_lastPrint < 0 || m_lastPrint >= m_count)
	//	m_lastPrint = -1;
#endif
}

void WifiList::writeWifiList()
{
#ifdef _WIN32
	// open base key
	HKEY baseKey;
	if(RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\miniMover\\", 0, NULL, 0, KEY_WRITE, NULL, &baseKey, NULL) == ERROR_SUCCESS)
	{
		// set index of last printer we connected to
		//if(RegSetKeyValueA(baseKey, NULL, "lastPrinterIndex", REG_DWORD, &m_lastPrint, sizeof(DWORD)) == ERROR_SUCCESS)
		{
			// open wifi subkey
			HKEY wifiKey;
			if(RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\miniMover\\wifi\\", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &wifiKey, NULL) == ERROR_SUCCESS)
			{
				// remove existing subkeys
				if(RegDeleteTreeA(wifiKey, NULL) == ERROR_SUCCESS)
				{
					// for each entry in the list
					char tpath[128];
					for(int i=0; i<m_count; i++)
					{
						// if entry is valid
						if( strlen(m_list[i].m_serialNum) > 0 &&
							strlen(m_list[i].m_ip) > 0 )
						{
							// create a subkey
							sprintf(tpath, "Software\\miniMover\\wifi\\%s\\", m_list[i].m_serialNum);
							HKEY entryKey;
							if(RegCreateKeyExA(HKEY_CURRENT_USER, tpath, 0, NULL, 0, KEY_WRITE, NULL, &entryKey, NULL) == ERROR_SUCCESS)
							{
								if(RegSetKeyValueA(entryKey, NULL, "ip", REG_SZ, &m_list[i].m_ip, strlen(m_list[i].m_ip)) == ERROR_SUCCESS)
								{
									// success
								}
								RegCloseKey(entryKey);
							}
						}
					}
				}
				RegCloseKey(wifiKey);
			}
		}
		RegCloseKey(baseKey);
	}
#endif
}

WifiEntry* WifiList::findEntry(const char *serialNum, bool addIfNotFound)
{
	// return if found
	for(int i=0; i<m_count; i++)
	{
		if(0 == strcmp(serialNum, m_list[i].m_serialNum))
			return &m_list[i];
	}
	
	// add if asked
	if(addIfNotFound && (m_count+1) < m_max)
	{
		m_count++;
		m_list[m_count-1].reset();
		return &m_list[m_count-1];
	}

	// give up
	return NULL;
}
