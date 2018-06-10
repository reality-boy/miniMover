
// includes

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <Windows.h>
#include <Windowsx.h>
#include <commdlg.h>
#include <time.h>

#include <stdlib.h>
#include <stdarg.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <mmsystem.h>
#include <strsafe.h>
#include <assert.h>
#include <shlobj.h>

#if defined(DEBUG) | defined(_DEBUG)
#include <crtdbg.h>
#endif

#include "Resource.h"

#include "debug.h"
#include "timer.h"
#include "serial.h"
#include "network.h"
#include "xyzv3.h"
#include "XYZPrinterList.h"
#include "XYZV3Thread.h"

// tell windows to give us a more modern look
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma warning(disable: 4996) // disable deprecated warning 
#pragma warning(disable: 4800)

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Comctl32.lib")

//-------------------------------------------
// globals

XYZV3 xyz;
Serial g_serial;
Socket g_soc;

WifiList g_wifiList;
int g_listWifiOffset = -1;
int g_listSerialOffset = -1;
XYZPrinterStatus g_prSt = { 0 };

int g_timerInterval = 500;
UINT_PTR g_timer = 0;

// set by XYZV3Thread.cpp
int g_printPct = 0;

bool g_threadRunning = false;
bool g_wifiOptionsEdited = false;

// controls
HWND hwndListInfo = NULL;

HCURSOR waitCursor;
HCURSOR defaultCursor;

//-------------------------------------------
// main dialog

void listAddLine(HWND hList, const char *format, ...)
{
	const static int BUFF_SIZE = 2048;
	char msgBuf[BUFF_SIZE];
	va_list arglist;

	if(hList)
	{
		va_start(arglist, format);
		vsnprintf(msgBuf, sizeof(msgBuf), format, arglist);
		msgBuf[sizeof(msgBuf)-1] = '\0';
		va_end(arglist);

		SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)msgBuf);
	}
}

void MainDlgUpdateStatusList(HWND hDlg, const XYZPrinterStatus *st, const XYZPrinterInfo *inf)
{
	(void)hDlg;

	int d, h, m;

	if(hwndListInfo && st && inf)
	{
		// don't repaint till we are done drawing
		SetWindowRedraw(hwndListInfo, FALSE);

		int index = ListBox_GetTopIndex(hwndListInfo);
		SendMessage(hwndListInfo, LB_RESETCONTENT, 0, 0);

		listAddLine(hwndListInfo, "Name: %s", inf->screenName);
		//listAddLine(hwndListInfo, "Given name: %s", st->nMachineName);
		//listAddLine(hwndListInfo, "Model num: %s", inf->modelNum);
		listAddLine(hwndListInfo, "Model num: %s", st->pMachineModelNumber);
		listAddLine(hwndListInfo, "File id: %s", inf->fileNum);
		//listAddLine(hwndListInfo, "Serial num: %s", inf->serialNum);
		listAddLine(hwndListInfo, "Serial num: %s", st->iMachineSerialNum);

		listAddLine(hwndListInfo, "Filament serial: %s", st->wFilament1SerialNumber);

		if(st->wFilament1Color[0])
			listAddLine(hwndListInfo, "Filament color: %s", st->wFilament1Color);

		if(st->wFilamentCount > 1)
		{
			if(st->wFilament2SerialNumber[0])
				listAddLine(hwndListInfo, "Filament 2 serial: %s", st->wFilament2SerialNumber);

			if(st->wFilament2Color[0])
				listAddLine(hwndListInfo, "Filament 2 color: %s", st->wFilament2Color);
		}

		if(st->sOpenFilament)
			listAddLine(hwndListInfo, "Uses Open Filamen");

		if(st->fFilamentSpoolCount > 1 && st->fFilament2Remaining_mm > 0)
			listAddLine(hwndListInfo, "Filament remain: %0.2f m, %0.2f m", st->fFilament1Remaining_mm / 1000.0f, st->fFilament2Remaining_mm / 1000.0f);
		else
			listAddLine(hwndListInfo, "Filament remain: %0.2f m", st->fFilament1Remaining_mm / 1000.0f);

		if(st->hIsFilamentPLA)
			listAddLine(hwndListInfo, "Is PLA");

		if(st->kFilamentMaterialType > 0)
			listAddLine(hwndListInfo, "Filament Material Type: (%d) %s", st->kFilamentMaterialType, st->kFilamentMaterialTypeStr);

		listAddLine(hwndListInfo, "Nozzle serial: %s", st->XNozzle1SerialNumber);
		if(st->XNozzle2SerialNumber[0])
			listAddLine(hwndListInfo, "Nozzle 2 serial: %s", st->XNozzle2SerialNumber);
		listAddLine(hwndListInfo, "Firmware ver: %s", st->vFirmwareVersion);
		listAddLine(hwndListInfo, "Nozzle ID: %d, Diam %0.2f mm%s", st->XNozzleID, st->XNozzleDiameter_mm, (st->XNozzleIsLaser) ? ", laser" : "");

		listAddLine(hwndListInfo, "Build volume: %d l %d w %d h", inf->length, inf->width, inf->height);
		if(inf->fileIsV5)
			listAddLine(hwndListInfo, "File is v5");
		if(inf->fileIsZip)
			listAddLine(hwndListInfo, "File is zip");
		//if(inf->comIsV3)
		//	listAddLine(hwndListInfo, "Com is v3");

		listAddLine(hwndListInfo, "Packet size: %d", st->oPacketSize);

		if(st->cCalibIsValid)
			listAddLine(hwndListInfo, "Calib: %d,%d,%d,%d,%d,%d,%d,%d,%d", 
								st->cCalib[0], st->cCalib[1], st->cCalib[2],
								st->cCalib[3], st->cCalib[4], st->cCalib[5],
								st->cCalib[6], st->cCalib[7], st->cCalib[8]);
		listAddLine(hwndListInfo, "Auto level: %d", st->oAutoLevelEnabled);
		listAddLine(hwndListInfo, "Buzzer: %d", st->sBuzzerEnabled);
		listAddLine(hwndListInfo, "Z Offset: %d", st->zOffset);
		listAddLine(hwndListInfo, "oT: %d, oC: %d, Fd: %d, Fm: %d", 
			st->oT, st->oC, st->sFd, st->sFm);

		if(st->sFrontDoor)
			listAddLine(hwndListInfo, "Front Door Open");
		if(st->sTopDoor)
			listAddLine(hwndListInfo, "Top Door Open");
		if(st->sHasLazer)
			listAddLine(hwndListInfo, "Has Lazer");
		if(st->sButton)
			listAddLine(hwndListInfo, "Has button");
		if(st->sSDCard)
			listAddLine(hwndListInfo, "Has SD card");

		d =  st->LPrinterLifetimePowerOnTime_min / (60 * 24);
		h = (st->LPrinterLifetimePowerOnTime_min / 60) % 24;
		m =  st->LPrinterLifetimePowerOnTime_min % 60;
		listAddLine(hwndListInfo, "Lifetime power on time: %d d %d h %d m", d, h, m);

		if(st->LPrinterLastPowerOnTime_min > 0)
		{
			d =  st->LPrinterLastPowerOnTime_min / (60 * 24);
			h = (st->LPrinterLastPowerOnTime_min / 60) % 24;
			m =  st->LPrinterLastPowerOnTime_min % 60;
			listAddLine(hwndListInfo, "Last print time: %d d %d h %d m", d, h, m);
		}

		//st->LExtruderCount
		// I suspect there are two of these
		d =  st->LExtruderLifetimePowerOnTime_min / (60 * 24);
		h = (st->LExtruderLifetimePowerOnTime_min / 60) % 24;
		m =  st->LExtruderLifetimePowerOnTime_min % 60;
		listAddLine(hwndListInfo, "Extruder total time: %d d %d h %d m", d, h, m);

		listAddLine(hwndListInfo, "Lang: %s", st->lLang);

		if(st->mVal[0] != 0)
			listAddLine(hwndListInfo, "mVal: %d", st->mVal);

		if(st->GLastUsed[0])
			listAddLine(hwndListInfo, "LastUsed: %s", st->GLastUsed);

		if(st->VString[0])
			listAddLine(hwndListInfo, "VString: %s", st->VString);

		if(st->WSSID[0])
		{
			listAddLine(hwndListInfo, "Wifi SSID: %s", st->WSSID);
			listAddLine(hwndListInfo, "Wifi BSSID: %s", st->WBSSID);
			listAddLine(hwndListInfo, "Wifi Channel: %s", st->WChannel);
			listAddLine(hwndListInfo, "Wifi Rssi: %s", st->WRssiValue);
			listAddLine(hwndListInfo, "Wifi PHY: %s", st->WPHY);
			listAddLine(hwndListInfo, "Wifi Security: %s", st->WSecurity);
		}

		if(st->N4NetSSID[0])
		{
			listAddLine(hwndListInfo, "Wifi IP: %s", st->N4NetIP);
			listAddLine(hwndListInfo, "Wifi SSID: %s", st->N4NetSSID);
			listAddLine(hwndListInfo, "Wifi Chan: %s", st->N4NetChan);
			listAddLine(hwndListInfo, "Wifi MAC: %s", st->N4NetMAC);
			listAddLine(hwndListInfo, "Wifi Rssi: %d dB - %d %%", st->N4NetRssiValue, st->N4NetRssiValuePct);
		}

		// print status info

		if(st->bBedActualTemp_C > 20 || st->OBedTargetTemp_C > 0)
			listAddLine(hwndListInfo, "Bed temp: %d C, target temp: %d C", st->bBedActualTemp_C, st->OBedTargetTemp_C);

		if(st->tExtruderCount > 1 && st->tExtruder2ActualTemp_C > 0)
			listAddLine(hwndListInfo, "Extruder temp: %d C, %d C / %d C", st->tExtruder1ActualTemp_C, st->tExtruder2ActualTemp_C, st->tExtruderTargetTemp_C);
		else
			listAddLine(hwndListInfo, "Extruder temp: %d C / %d C", st->tExtruder1ActualTemp_C, st->tExtruderTargetTemp_C);

		bool isPrinting = st->jPrinterState >= PRINT_INITIAL && st->jPrinterState <= PRINT_PRINTING;
		if(!isPrinting && st->dPrintPercentComplete == 0 && st->dPrintElapsedTime_m == 0 && st->dPrintTimeLeft_m == 0)
		{
				//listAddLine(hwndListInfo, "No job running");
		}
		else
		{
			listAddLine(hwndListInfo, "Print pct complete: %d %%", st->dPrintPercentComplete);

			h = st->dPrintElapsedTime_m / 60;
			m = st->dPrintElapsedTime_m % 60;
			listAddLine(hwndListInfo, "Print elapsed: %d h %d m", h, m);

			if(st->dPrintTimeLeft_m >= 0)
			{
				h = st->dPrintTimeLeft_m / 60;
				m = st->dPrintTimeLeft_m % 60;
				listAddLine(hwndListInfo, "Print remain: %d h %d m", h, m);
			}
			else
				listAddLine(hwndListInfo, "Print remain: unknown");
		}

		if(st->eErrorStatus != 0)
			listAddLine(hwndListInfo, "Error: (0x%08x) %s", st->eErrorStatus, st->eErrorStatusStr);
		listAddLine(hwndListInfo, "Status: (%d:%d) %s", st->jPrinterState, st->jPrinterSubState, st->jPrinterStateStr);

		ListBox_SetTopIndex(hwndListInfo, index);
		// now repaint all at once
		SetWindowRedraw(hwndListInfo, TRUE);
	}
}

void MainDlgSetStatus(HWND hDlg, const char *msg)
{
	debugPrint(DBG_LOG, "status: %s", msg);
	SendDlgItemMessage(hDlg, IDC_STATIC_STATUS, WM_SETTEXT, 0, (LPARAM)msg);
}

void setZOffset(HWND hDlg)
{
	SetCursor(waitCursor);
	MainDlgSetStatus(hDlg, "set z-offset");
	if(xyz.setZOffset(GetDlgItemInt(hDlg, IDC_EDIT_ZOFF, NULL, false)))
		MainDlgSetStatus(hDlg, "set z-offset complete");
	else
		MainDlgSetStatus(hDlg, "set z-offset failed");
	SetCursor(defaultCursor);
}

void setMachineName(HWND hDlg)
{
	SetCursor(waitCursor);
	MainDlgSetStatus(hDlg, "set machine name");
	char tstr[64];
	GetDlgItemTextA(hDlg, IDC_EDIT_MACHINE_NAME, tstr, sizeof(tstr));
	if(xyz.setMachineName(tstr))
		MainDlgSetStatus(hDlg, "set machine name complete");
	else
		MainDlgSetStatus(hDlg, "set machine name failed");
	SetCursor(defaultCursor);
}

int getMoveDist(HWND hDlg)
{
	int t = SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_GETCURSEL, 0, 0);
	if(t == 0) return 1;
	if(t == 1) return 10;
	if(t == 2) return 100;
	return 1;
}

void MainDlgUpdateComDropdown(HWND hDlg)
{
	int ent = 0;
	// clear out dropdown
	SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_RESETCONTENT, 0, 0);

	// add the 'auto' entry
	SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_ADDSTRING, 0, (LPARAM)"Auto");
	ent++;

	// and default to the auto entry
	SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_SETCURSEL, 0, 0);

	// add in network printers
	g_listWifiOffset = ent;
	char tstr[512];
	for(int i=0; i<g_wifiList.m_count; i++)
	{
		sprintf(tstr, "%s (%s)", g_wifiList.m_list[i].m_name, g_wifiList.m_list[i].m_ip);
		SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_ADDSTRING, 0, (LPARAM)tstr);
		ent++;
	}

	// add in serial printers
	g_listSerialOffset = ent;
	SerialHelper::queryForPorts("XYZ");
	int count = SerialHelper::getPortCount();
	for(int i=0; i< count; i++)
	{
		SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_ADDSTRING, 0, (LPARAM)SerialHelper::getPortDisplayName(i));
		ent++;
	}
}

void MainDlgUpdateModelDropdown(HWND hDlg)
{
	SendDlgItemMessage(hDlg, IDC_COMBO_MODEL, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hDlg, IDC_COMBO_MODEL, CB_ADDSTRING, 0, (LPARAM)"Auto");
	for(int i=0; i < XYZV3::getInfoCount(); i++)
		SendDlgItemMessage(hDlg, IDC_COMBO_MODEL, CB_ADDSTRING, 0, (LPARAM)XYZV3::indexToInfo(i)->screenName);
	SendDlgItemMessage(hDlg, IDC_COMBO_MODEL, CB_SETCURSEL, 0, 0);
}

void MainDlgUpdate(HWND hDlg)
{
	debugReduceNoise(true);
	// don't set wait cursor since this triggers 2x a second
	if(!g_threadRunning && xyz.queryStatus())
	{
		const XYZPrinterStatus *st = xyz.getPrinterStatus();
		const XYZPrinterInfo *inf = xyz.getPrinterInfo();

		if(st->isValid)
		{
			MainDlgUpdateStatusList(hDlg, st, inf);

			SendDlgItemMessage(hDlg, IDC_CHECK_BUZZER, BM_SETCHECK, (WPARAM)(st->sBuzzerEnabled) ? BST_CHECKED : BST_UNCHECKED, 0);
			SendDlgItemMessage(hDlg, IDC_CHECK_AUTO, BM_SETCHECK, (WPARAM)(st->oAutoLevelEnabled) ? BST_CHECKED : BST_UNCHECKED, 0);

			// only update if changed
			if(g_prSt.zOffset != st->zOffset)
				SetDlgItemInt(hDlg, IDC_EDIT_ZOFF, st->zOffset, false);

			//****FixMe, save these off in the registry so we can
			// detect the printer when the usb is disconnected
			// st->N4NetIP
			// st->N4NetSSID
			// st->nMachineName
			// 
			// or check out
			// Computer\HKEY_CURRENT_USER\Software\XYZware\xyzsetting
			//   DefaultIP     192.168.0.20
			//   DefaultPort   COM3
			//   DefaultPrinter  XYZprinting da Vinci miniMaker
			//   LastPrinterSN 3FM1XPUS5CA68P0591
			// Computer\HKEY_CURRENT_USER\Software\XYZware\xyzsetting\3FM1XPUS5CA68P0591

			// I believe the W functions represent wifi network as configured on the machine
			// and 4 functions represent the network as connected
			if(!g_wifiOptionsEdited)
			{
				const char *prSSID = (g_prSt.WSSID[0]) ? g_prSt.WSSID : g_prSt.N4NetSSID;
				const char *SSID = (st->WSSID[0]) ? st->WSSID : st->N4NetSSID;
				// only update if changed
				if(0!=strcmp(prSSID, SSID))
					SetDlgItemTextA(hDlg, IDC_EDIT_WIFI_SSID, SSID);

				const char *prChan = (g_prSt.WChannel[0]) ? g_prSt.WChannel : g_prSt.N4NetChan;
				const char *chan = (st->WChannel[0]) ? st->WChannel : st->N4NetChan;
				// only update if changed
				if(0!=strcmp(prChan, chan))
					SetDlgItemTextA(hDlg, IDC_EDIT_WIFI_CHAN, chan);

				/*
				if(*SSID || *chan)
					SetDlgItemTextA(hDlg, IDC_EDIT_WIFI_PASSWD, "******");
				else
					SetDlgItemTextA(hDlg, IDC_EDIT_WIFI_PASSWD, "");
				*/
			}

			//****FixMe, what one matches this?
			//SetDlgItemTextA(hDlg, IDC_COMBO_ENERGY_SAVING, st->???);

			// only update if changed
			if(0 != strcmp(g_prSt.nMachineName, st->nMachineName))
				SetDlgItemTextA(hDlg, IDC_EDIT_MACHINE_NAME, st->nMachineName);

			int pct = max(g_printPct, st->dPrintPercentComplete);
			SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_SETPOS, pct, 0);

			if(0 != strcmp(g_prSt.lLang, st->lLang))
			{
				int id = 0;
				for(id=0; id<XYZPrintingLangCount; id++)
					if(0 == strcmp(st->lLang, XYZPrintingLang[id].abrv))
						break;
				// default to en if not found
				if(id == XYZPrintingLangCount)
					id = 0;

				SendDlgItemMessage(hDlg, IDC_COMBO_LANGUAGE, CB_SETCURSEL, id, 0);
			}

			// if network info changed
			if(0 != strcmp(g_prSt.iMachineSerialNum, st->iMachineSerialNum) ||
			   0 != strcmp(g_prSt.N4NetIP, st->N4NetIP) )
			{
				// find entry, adding new if not found
				WifiEntry *ent = g_wifiList.findEntry(st->iMachineSerialNum, true);
				if(ent)
					ent->set(st->iMachineSerialNum, st->N4NetIP, inf->screenName);
			}

			// copy to backup
			memcpy(&g_prSt, st, sizeof(g_prSt));
		}
	}
	else
		SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_SETPOS, g_printPct, 0);
	debugReduceNoise(false);
}

void MainDlgConnect(HWND hDlg)
{
	// get dropdown entry
	int comID = SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_GETCURSEL, 0, 0);

	// if error, default to auto
	if(comID == CB_ERR)
		comID = 0;

	// clear out cache of printer previous status
	memset(&g_prSt, 0, sizeof(g_prSt));
	g_wifiOptionsEdited = false;

	// close old connection
	Stream *s = xyz.setStream(NULL);
	if(s) s->closeStream();
	MainDlgSetStatus(hDlg, "not connected");

	// if auto detect
	if(comID == 0)
	{
		// try serial first
		int id = SerialHelper::queryForPorts("XYZ");
		if(id >= 0 && g_serial.openSerial(SerialHelper::getPortDeviceName(id), 115200))
		{
			Stream *s = xyz.setStream(&g_serial);
			if(s) s->closeStream();
			MainDlgSetStatus(hDlg, "connected");
		}
		// fall back to wifi if no serial
		/*
		else if(g_wifiList.m_count > 0)
		{
			if(g_soc.openSocket(g_wifiList.m_list[0].m_ip, 9100))
			{
				Sleep(500);
				Stream *s = xyz.setStream(&g_soc);
				if(s) s->closeStream();
				MainDlgSetStatus(hDlg, "connected");
			}
		}
		*/
	}
	// else if serial port
	else if(comID >= g_listSerialOffset)
	{
		int id = comID - g_listSerialOffset;
		if(id >= 0 && g_serial.openSerial(SerialHelper::getPortDeviceName(id), 115200))
		{
			Stream *s = xyz.setStream(&g_serial);
			if(s) s->closeStream();
			MainDlgSetStatus(hDlg, "connected");
		}
	}
	// else if wifi
	else if(comID >= g_listWifiOffset && comID < g_listSerialOffset)
	{
		int id = comID - g_listWifiOffset;
		if(id >=0 && id < g_wifiList.m_count)
		{
			if(g_soc.openSocket(g_wifiList.m_list[id].m_ip, 9100))
			{
				Sleep(500);
				Stream *s = xyz.setStream(&g_soc);
				if(s) s->closeStream();
				MainDlgSetStatus(hDlg, "connected");
			}
		}
	}
}

BOOL CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int t;
	switch(msg) 
	{
	case WM_INITDIALOG:
		defaultCursor = LoadCursor(NULL, IDC_ARROW);    // default cursor
		waitCursor = LoadCursor(NULL, IDC_WAIT);     // wait cursor

		// only do this once at startup, before initializing the dropdown
		g_wifiList.readWifiList();

		MainDlgUpdateComDropdown(hDlg);
		MainDlgUpdateModelDropdown(hDlg);
		MainDlgConnect(hDlg);

		for(int i=0; i<XYZPrintingLangCount; i++)
			SendDlgItemMessage(hDlg, IDC_COMBO_LANGUAGE, CB_ADDSTRING, 0, (LPARAM)XYZPrintingLang[i].desc);
		SendDlgItemMessage(hDlg, IDC_COMBO_LANGUAGE, CB_SETCURSEL, 0, 0); // default to something

		SendDlgItemMessage(hDlg, IDC_COMBO_ENERGY_SAVING, CB_ADDSTRING, 0, (LPARAM)"off");
		SendDlgItemMessage(hDlg, IDC_COMBO_ENERGY_SAVING, CB_ADDSTRING, 0, (LPARAM)"3 min");
		SendDlgItemMessage(hDlg, IDC_COMBO_ENERGY_SAVING, CB_ADDSTRING, 0, (LPARAM)"6 min");
		SendDlgItemMessage(hDlg, IDC_COMBO_ENERGY_SAVING, CB_SETCURSEL, 1, 0); // default to something

		SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_ADDSTRING, 0, (LPARAM)"1 mm");
		SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_ADDSTRING, 0, (LPARAM)"10 mm");
		SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_ADDSTRING, 0, (LPARAM)"100 mm");
		SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_SETCURSEL, 1, 0); // default to something

		hwndListInfo = GetDlgItem(hDlg, IDC_LIST_STATUS);

		g_timer = SetTimer(hDlg, NULL, g_timerInterval, NULL);

		SendDlgItemMessage(hDlg, IDC_SPIN_ZOFF, UDM_SETRANGE, 0, MAKELONG( 1000, 1));
		SendDlgItemMessage(hDlg, IDC_SPIN_WIFI_CHAN, UDM_SETRANGE, 0, MAKELONG( 14, 1));

		// force a refresh, without waiting for timer
		MainDlgUpdate(hDlg);
		break;

	case WM_DESTROY:
		// Cleanup everything
		g_wifiList.writeWifiList();
		KillTimer(hDlg, g_timer);
		{
			Stream *s = xyz.setStream(NULL);
			if(s) s->closeStream();
		}
		PostQuitMessage(0);
		break;

	case WM_CLOSE:
		DestroyWindow(hDlg);
		return true;

	case WM_ACTIVATE:
		break;

	case WM_SETCURSOR:
		if(g_threadRunning)
			SetCursor(waitCursor);
//		else
//			SetCursor(defaultCursor);
		break;

	case XYZ_THREAD_DONE:
		g_threadRunning = false;
		if(wParam)
			MainDlgSetStatus(hDlg, "process complete");
		else
			MainDlgSetStatus(hDlg, "process failed");
		SetCursor(defaultCursor);
		break;

	case  WM_TIMER:
		MainDlgUpdate(hDlg);
		break;

	case  WM_VSCROLL:
		if(LOWORD(wParam) == SB_ENDSCROLL)
		{
			if((HWND)lParam ==  GetDlgItem(hDlg, IDC_SPIN_ZOFF)) 
				setZOffset(hDlg);
		}
		break;

	case WM_COMMAND:

		if(g_threadRunning && LOWORD(wParam) != IDCANCEL)
			break;

		switch(LOWORD(wParam))
		{
		case IDOK:
			if(GetFocus() == GetDlgItem(hDlg, IDC_EDIT_ZOFF))
				setZOffset(hDlg);
			else if(GetFocus() == GetDlgItem(hDlg, IDC_EDIT_MACHINE_NAME))
				setMachineName(hDlg);
			break;

		case IDCANCEL:
			EndDialog(hDlg, 0);
			break;

		case IDC_BUTTON_PRINT:
			MainDlgSetStatus(hDlg, "printing file");
			SetCursor(waitCursor);

			g_threadRunning = handlePrintFile(hDlg, xyz);
			if(!g_threadRunning)
			{
				SetCursor(defaultCursor);
				MainDlgSetStatus(hDlg, "print failed");
			}
			break;

		case IDC_BUTTON_PAUSE:
			MainDlgSetStatus(hDlg, "pause print");
			SetCursor(waitCursor);
			if(xyz.pausePrint())
				MainDlgSetStatus(hDlg, "print paused");
			else
				MainDlgSetStatus(hDlg, "pause print failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_RESUME:
			MainDlgSetStatus(hDlg, "resume print");
			SetCursor(waitCursor);
			if(xyz.resumePrint())
				MainDlgSetStatus(hDlg, "print resumed");
			else
				MainDlgSetStatus(hDlg, "resume print failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_CANCEL:
			MainDlgSetStatus(hDlg, "cancel print");
			SetCursor(waitCursor);
			if(xyz.cancelPrint())
				MainDlgSetStatus(hDlg, "print canceld");
			else
				MainDlgSetStatus(hDlg, "cancel print failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_CONVERT:
			MainDlgSetStatus(hDlg, "converting file");
			SetCursor(waitCursor);

			t = SendDlgItemMessage(hDlg, IDC_COMBO_MODEL, CB_GETCURSEL, 0, 0);
			g_threadRunning = handleConvertFile(hDlg, xyz, t-1);
			if(!g_threadRunning)
			{
				SetCursor(defaultCursor);
				MainDlgSetStatus(hDlg, "converting failed");
			}
			break;

		case IDC_BUTTON_LOAD:
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "loading filament");
			if( xyz.loadFilamentStart() &&
				xyz.loadFilamentRun())
			{
				MessageBox(NULL, "Hit ok when filliment comes out of nozzle.", "Load Filament", MB_OK);
				if(xyz.loadFilamentCancel())
					MainDlgSetStatus(hDlg, "loading filament complete");
				else
					MainDlgSetStatus(hDlg, "loading filament failed");
			}
			else
				MainDlgSetStatus(hDlg, "loading filament failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_UNLOAD: 
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "unloading filament");
			if( xyz.unloadFilamentStart() &&
				xyz.unloadFilamentRun())
				MainDlgSetStatus(hDlg, "unloading filament complete");
			else
				MainDlgSetStatus(hDlg, "unloading filament failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_CLEAN: 
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "cleaning nozzle");
			if( xyz.cleanNozzleStart() &&
				xyz.cleanNozzleRun())
			{
				MessageBox(NULL, "Hit ok after cleaning nozzle.", "Clean Nozzle", MB_OK);
				if(xyz.cleanNozzleCancel())
					MainDlgSetStatus(hDlg, "cleaning nozzle complete");
				else
					MainDlgSetStatus(hDlg, "cleaning nozzle failed");
			}
			else
				MainDlgSetStatus(hDlg, "cleaning nozzle failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_CALIB: 
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "calibrating bed");
			if(xyz.calibrateBedStart()) 
			{
				MessageBox(NULL, "Lower detector and hit ok.", "Calibrate Bed", MB_OK);
				if( xyz.calibrateBedDetectorLowered() &&
					xyz.calibrateBedRun())
				{
					MessageBox(NULL, "Raise detector and hit ok.", "Calibrate Bed", MB_OK);
					if(xyz.calibrateBedFinish())
						MainDlgSetStatus(hDlg, "calibrating bed complete");
					else
						MainDlgSetStatus(hDlg, "calibrating bed failed");
				}
				else
					MainDlgSetStatus(hDlg, "calibrating bed failed");
			}
			else
				MainDlgSetStatus(hDlg, "calibrating bed failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_HOME: 
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "homing printer");
			if( xyz.homePrinterStart() &&
				xyz.homePrinterRun()) 
				MainDlgSetStatus(hDlg, "homing printer complete");
			else
				MainDlgSetStatus(hDlg, "homing printer failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_XP: 
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "move x");
			if( xyz.jogPrinterStart('x', getMoveDist(hDlg)) &&
				xyz.jogPrinterRun())
				MainDlgSetStatus(hDlg, "move x complete");
			else
				MainDlgSetStatus(hDlg, "move x failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_XM: 
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "move x");
			if( xyz.jogPrinterStart('x', -getMoveDist(hDlg)) &&
				xyz.jogPrinterRun())
				MainDlgSetStatus(hDlg, "move x complete");
			else
				MainDlgSetStatus(hDlg, "move x failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_YP: 
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "move y");
			if( xyz.jogPrinterStart('y', getMoveDist(hDlg)) &&
				xyz.jogPrinterRun())
				MainDlgSetStatus(hDlg, "move y complete");
			else
				MainDlgSetStatus(hDlg, "move y failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_YM: 
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "move y");
			if( xyz.jogPrinterStart('y', -getMoveDist(hDlg)) &&
				xyz.jogPrinterRun())
				MainDlgSetStatus(hDlg, "move y complete");
			else
				MainDlgSetStatus(hDlg, "move y failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_ZP: 
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "move z");
			if( xyz.jogPrinterStart('z', getMoveDist(hDlg)) &&
				xyz.jogPrinterRun())
				MainDlgSetStatus(hDlg, "move z complete");
			else
				MainDlgSetStatus(hDlg, "move z failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_ZM: 
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "move z");
			if( xyz.jogPrinterStart('z', -getMoveDist(hDlg)) &&
				xyz.jogPrinterRun())
				MainDlgSetStatus(hDlg, "move z complete");
			else
				MainDlgSetStatus(hDlg, "move z failed");
			SetCursor(defaultCursor);
			break;

		case IDC_BUTTON_WIFI_AUTO:
			{
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "auto detecting wifi");

				char ssid[64];
				char password[64];
				int chan;

				if(autoDetectWifi(ssid, password, chan))
				{
					SetDlgItemTextA(hDlg, IDC_EDIT_WIFI_SSID, ssid);
					SetDlgItemInt(hDlg, IDC_EDIT_WIFI_CHAN, chan, false);

					if('\0' != password[0])
					{
						SetDlgItemTextA(hDlg, IDC_EDIT_WIFI_PASSWD, password);
						MainDlgSetStatus(hDlg, "auto detect wifi complete");
					}
					else
					{
						MainDlgSetStatus(hDlg, "failed to detect wifi password");
						SetDlgItemTextA(hDlg, IDC_EDIT_WIFI_PASSWD, "");
					}

					g_wifiOptionsEdited = true;
				}
				else
					MainDlgSetStatus(hDlg, "failed to detect wifi network");

				SetCursor(defaultCursor);
			}
			break;

		case IDC_EDIT_WIFI_SSID:
		case IDC_EDIT_WIFI_PASSWD:
		case IDC_EDIT_WIFI_CHAN:
		case IDC_SPIN_WIFI_CHAN:
			// don't override user edits with status updates from printer
			g_wifiOptionsEdited = true;
			break;

		case IDC_BUTTON_WIFI_SET:
			{
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "set wifi parameters");

				char ssid[64] = "";
				char password[64] = "";
				int chan = -1;

				GetDlgItemTextA(hDlg, IDC_EDIT_WIFI_SSID, ssid, sizeof(ssid));
				GetDlgItemTextA(hDlg, IDC_EDIT_WIFI_PASSWD, password, sizeof(password));
				chan = GetDlgItemInt(hDlg, IDC_EDIT_WIFI_CHAN, NULL, false);

				bool noPass = (0 == strcmp(password, "******"));
				if(!*ssid || !*password || noPass || chan < 0)
				{
					MessageBox(NULL, "Invalid wifi settings.", "miniMover", MB_OK);
					MainDlgSetStatus(hDlg, "set wifi parameters failed");
				}
				else if(xyz.setWifi(ssid, password, chan))
				{
					MainDlgSetStatus(hDlg, "set wifi parameters complete");
					g_wifiOptionsEdited = false;
				}
				else
					MainDlgSetStatus(hDlg, "set wifi parameters failed");

				SetCursor(defaultCursor);
			}
			break;

		case IDC_BUTTON_RESET:
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "reset options");
			if(xyz.restoreDefaults())
			{
				g_wifiOptionsEdited = false;
				MainDlgSetStatus(hDlg, "reset options complete");
			}
			else
				MainDlgSetStatus(hDlg, "reset options failed");
			SetCursor(defaultCursor);
			break;

		case IDC_EDIT_ZOFF:
			if(HIWORD(wParam) == EN_KILLFOCUS)
				setZOffset(hDlg);
			break;

		case IDC_EDIT_MACHINE_NAME:
			if(HIWORD(wParam) == EN_KILLFOCUS)
				setMachineName(hDlg);
			break;

		case IDC_COMBO_ENERGY_SAVING:
			if(HIWORD(wParam) == CBN_SELCHANGE)
			{
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "set energy saving");
				int id = SendDlgItemMessage(hDlg, IDC_COMBO_LANGUAGE, CB_GETCURSEL, 0, 0);
				if(xyz.setEnergySaving(id * 3))
					MainDlgSetStatus(hDlg, "set energy saving complete");
				else
					MainDlgSetStatus(hDlg, "set energy saving failed");
				SetCursor(defaultCursor);
			}
			break;

		case IDC_COMBO_LANGUAGE:
			if(HIWORD(wParam) == CBN_SELCHANGE)
			{
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "set language");
				int id = SendDlgItemMessage(hDlg, IDC_COMBO_LANGUAGE, CB_GETCURSEL, 0, 0);
				if(xyz.setLanguage(XYZPrintingLang[id].abrv))
					MainDlgSetStatus(hDlg, "set language complete");
				else
					MainDlgSetStatus(hDlg, "set language failed");
				SetCursor(defaultCursor);
			}
			break;

		case IDC_COMBO_PORT:
			if(HIWORD(wParam) == CBN_SELCHANGE)
				MainDlgConnect(hDlg);
			else if(HIWORD(wParam) == CBN_DROPDOWN)
				MainDlgUpdateComDropdown(hDlg);
			break;

		case IDC_CHECK_AUTO:
			SetCursor(waitCursor);
			MainDlgSetStatus(hDlg, "toggle auto level");
			if (SendDlgItemMessage(hDlg, IDC_CHECK_AUTO, BM_GETCHECK, 0, 0)) 
				xyz.setAutoLevel(true);
			else
				xyz.setAutoLevel(false);
			SetCursor(defaultCursor);
			break;

		case IDC_CHECK_BUZZER:
			MainDlgSetStatus(hDlg, "toggle buzzer");
			SetCursor(waitCursor);
			if (SendDlgItemMessage(hDlg, IDC_CHECK_BUZZER, BM_GETCHECK, 0, 0)) 
				xyz.setBuzzer(true);
			else
				xyz.setBuzzer(false);
			SetCursor(defaultCursor);
			break;

		default:
			return FALSE; // Message not handled 
		}
		break;

	default:
		return FALSE; // Message not handled 
	}

	return TRUE; // Message handled 
}

//-------------------------------------------

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR lpCmdLine, INT)
{
	debugInit();

	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	//****FixMe, process command line
	(void)lpCmdLine;
	//CommandLineToArgv()

	INITCOMMONCONTROLSEX ex;
	ex.dwSize = sizeof(ex);
	ex.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&ex);

	timeBeginPeriod(1); // get 1 millisecond timers

	HWND hDlg = CreateDialog(hInst, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, MainDlgProc);
	if(hDlg)
	{
		MSG msg;
		while(GetMessage(&msg, 0, 0, 0) > 0)
		{
			if(!IsDialogMessage(hDlg, &msg)) 
			{
				TranslateMessage(&msg); /* translate virtual-key messages */
				DispatchMessage(&msg); /* send it to dialog procedure */
			}
		}

		//do shut down code here
		DestroyWindow(hDlg);
	}

	timeEndPeriod(1); // release 1 millisecond timer

	debugFinalize();

	return 0;
}
