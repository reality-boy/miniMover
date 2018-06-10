#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <Windows.h>

#include <stdlib.h>
#include <stdio.h>

#include "XYZPrinterList.h"

#pragma warning(disable: 4996) // disable deprecated warning 

void WifiEntry::reset()
{
	m_name[0] = '\0';
	m_serialNum[0] = '\0';
	m_ip[0] = '\0';
}

void WifiEntry::set(const char *serialNum, const char *ip, const char *scrName)
{
	if(serialNum && ip && scrName)
	{
		strcpy(m_serialNum, serialNum);
		strcpy(m_ip, ip);
		strcpy(m_name, scrName);
	}
	else
		reset();
}

bool WifiList::readXYZLastPrint(WifiEntry *ent)
{
	bool success = false;

	if(ent)
	{
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
						// printer screen name
						len = WifiEntry::m_len;
						if(RegQueryValueExA(key, "printerType", NULL, NULL, (LPBYTE)ent->m_name, &len) == ERROR_SUCCESS)
						{
							success = true;
						}
					}
				}
			}

			RegCloseKey(key);
		}

		// zero out strings if not found
		if(!success)
		{
			ent->m_ip[0] = '\0';
			ent->m_name[0] = '\0';
			ent->m_serialNum[0] = '\0';
		}
	}

	return success;
}

void WifiList::readWifiList()
{
	m_count = 0;
	m_lastPrint = -1;

	// open base key
	HKEY baseKey;
	if(RegOpenKeyA(HKEY_CURRENT_USER, "Software\\miniMover\\", &baseKey) == ERROR_SUCCESS)
	{
		// get index of last printer we connected to
		DWORD len = 32;
		char tstr[32];
		if(RegQueryValueExA(baseKey, "lastPrinterIndex", NULL, NULL, (LPBYTE)tstr, &len) == ERROR_SUCCESS)
		{
			// open wifi subkey
			m_lastPrint = atoi(tstr);
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
						if(RegQueryValueExA(entryKey, "ip", NULL, NULL, (LPBYTE)m_list[m_count].m_ip, &len) == ERROR_SUCCESS)
						{
							len = WifiEntry::m_len;
							if(RegQueryValueExA(entryKey, "screenName", NULL, NULL, (LPBYTE)m_list[m_count].m_name, &len) == ERROR_SUCCESS)
							{
								m_count++;
								found = true;
							}
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
			m_list[m_count].set(ent.m_serialNum, ent.m_ip, ent.m_name);
			m_count++;
		}
	}

	// if last print does not point to a valid entry, set it to unknown
	if(m_lastPrint < 0 || m_lastPrint >= m_count)
		m_lastPrint = -1;
}

void WifiList::writeWifiList()
{
	// open base key
	HKEY baseKey;
	if(RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\miniMover\\", 0, NULL, 0, KEY_WRITE, NULL, &baseKey, NULL) == ERROR_SUCCESS)
	{
		// set index of last printer we connected to
		if(RegSetKeyValueA(baseKey, NULL, "lastPrinterIndex", REG_DWORD, &m_lastPrint, sizeof(DWORD)) == ERROR_SUCCESS)
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
						// create a subkey
						sprintf(tpath, "Software\\miniMover\\wifi\\%s\\", m_list[i].m_serialNum);
						HKEY entryKey;
						if(RegCreateKeyExA(HKEY_CURRENT_USER, tpath, 0, NULL, 0, KEY_WRITE, NULL, &entryKey, NULL) == ERROR_SUCCESS)
						{
							if(RegSetKeyValueA(entryKey, NULL, "ip", REG_SZ, &m_list[i].m_ip, strlen(m_list[i].m_ip)) == ERROR_SUCCESS)
							{
								if(RegSetKeyValueA(entryKey, NULL, "screenName", REG_SZ, &m_list[i].m_name, strlen(m_list[i].m_ip)) == ERROR_SUCCESS)
								{
									// success
								}
							}
							RegCloseKey(entryKey);
						}
					}
				}
				RegCloseKey(wifiKey);
			}
		}
		RegCloseKey(baseKey);
	}
}

WifiEntry* WifiList::findEntry(const char *serialNum, bool addIfNotFound)
{
	// return if found
	for(int i=0; i<m_count; i++)
		if(0 == strcmp(serialNum, m_list[i].m_serialNum))
			return &m_list[i];
	
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

//****FixMe, update list when wifi printers deteced

/*
if(RegSetValueEx(key, TEXT("value_name"), 0, REG_SZ, (LPBYTE)"value_data", strlen("value_data")*sizeof(char)) == ERROR_SUCCESS)
HKEY hKey;

RegCloseKey(hKey);

LONG WINAPI RegCreateKeyEx(
  _In_       HKEY                  hKey,
  _In_       LPCTSTR               lpSubKey,
  _Reserved_ DWORD                 Reserved,
  _In_opt_   LPTSTR                lpClass,
  _In_       DWORD                 dwOptions,
  _In_       REGSAM                samDesired,
  _In_opt_   LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  _Out_      PHKEY                 phkResult,
  _Out_opt_  LPDWORD               lpdwDisposition
);

LONG WINAPI RegEnumKeyEx(
  _In_        HKEY      hKey,
  _In_        DWORD     dwIndex,
  _Out_       LPTSTR    lpName,
  _Inout_     LPDWORD   lpcName,
  _Reserved_  LPDWORD   lpReserved,
  _Inout_     LPTSTR    lpClass,
  _Inout_opt_ LPDWORD   lpcClass,
  _Out_opt_   PFILETIME lpftLastWriteTime
);
*/

