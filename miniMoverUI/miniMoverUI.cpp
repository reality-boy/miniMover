
// includes

// tell windows to give us a more modern look
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

#pragma warning(disable: 4996) // disable deprecated warning 
#pragma warning(disable: 4800)

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Comctl32.lib")

#include "targetver.h"
#include <windows.h>
#include <Windowsx.h>
#include <commdlg.h>
#include <time.h>

#include <stdlib.h>
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

#include "timer.h"
#include "serial.h"
#include "debug.h"
#include "xyzv3.h"
#include "XYZV3Thread.h"

// globals

XYZV3 xyz;

int g_timerInterval = 500;

bool g_threadRunning = false;

const int g_maxPorts = 24;
int g_comIDtoPort[g_maxPorts] = {-1};

UINT_PTR g_timer = 0;
int g_printPct = 0;

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
		_vsnprintf(msgBuf, sizeof(msgBuf), format, arglist);
		msgBuf[sizeof(msgBuf)-1] = '\0';
		va_end(arglist);

		SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)msgBuf);
	}
}

void MainDlgUpdateStatusList(HWND hDlg, const XYZPrinterState *st, const XYZPrinterInfo *inf)
{
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

		listAddLine(hwndListInfo, "Fillament serial: %s", st->wFilament1SerialNumber);
		if(st->wFilamentCount > 1 && st->wFilament2SerialNumber[0])
			listAddLine(hwndListInfo, "Fillament 2 serial: %s", st->wFilament2SerialNumber);

		listAddLine(hwndListInfo, "Nozzle serial: %s", st->XNozzle1SerialNumber);
		if(st->XNozzle2SerialNumber[0])
			listAddLine(hwndListInfo, "Nozzle 2 serial: %s", st->XNozzle2SerialNumber);
		listAddLine(hwndListInfo, "Firmware ver: %s", st->vFirmwareVersion);
		listAddLine(hwndListInfo, "Nozzle ID: %d, Diam %0.2f mm", st->XNozzleID, st->XNozzleDiameter_mm);

		listAddLine(hwndListInfo, "Build volume: %d l %d w %d h", inf->length, inf->width, inf->height);
		if(inf->fileIsV5)
			listAddLine(hwndListInfo, "File is v5");
		if(inf->fileIsZip)
			listAddLine(hwndListInfo, "File is zip");
		//if(inf->comIsV3)
		//	listAddLine(hwndListInfo, "Com is v3");
		if(st->hIsFillamentPLA)
			listAddLine(hwndListInfo, "Is PLA");

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
		if(st->sOpenFilament)
			listAddLine(hwndListInfo, "Uses Open Filamen");
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

		if(st->kMaterialType > 0)
			listAddLine(hwndListInfo, "kMaterialType: %d", st->kMaterialType);

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
			listAddLine(hwndListInfo, "Wifi Rssi: %s", st->N4NetRssiValue);
		}

		// print status info

		if(st->bBedActualTemp_C > 20 || st->OBedTargetTemp_C > 0)
			listAddLine(hwndListInfo, "Bed temp: %d C, target temp: %d C", st->bBedActualTemp_C, st->OBedTargetTemp_C);

		if(st->tExtruderCount > 1 && st->tExtruder2ActualTemp_C > 0)
			listAddLine(hwndListInfo, "Extruder temp: %d C, %d C / %d C", st->tExtruder1ActualTemp_C, st->tExtruder2ActualTemp_C, st->tExtruderTargetTemp_C);
		else
			listAddLine(hwndListInfo, "Extruder temp: %d C / %d C", st->tExtruder1ActualTemp_C, st->tExtruderTargetTemp_C);

		if(st->fFillimantSpoolCount > 1 && st->fFillimant2Remaining_mm > 0)
			listAddLine(hwndListInfo, "Fillament remain: %0.2f m, %0.2f m", st->fFillimant1Remaining_mm / 1000.0f, st->fFillimant2Remaining_mm / 1000.0f);
		else
			listAddLine(hwndListInfo, "Fillament remain: %0.2f m", st->fFillimant1Remaining_mm / 1000.0f);

		bool isPrinting = st->jPrinterStatus >= PRINT_INITIAL && st->jPrinterStatus <= PRINT_PRINTING;
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
			listAddLine(hwndListInfo, "Error: %d : %s", st->eErrorStatus, st->eErrorStatusStr);
		listAddLine(hwndListInfo, "Status: (%d:%d) %s", st->jPrinterStatus, st->jPrinterSubStatus, st->jPrinterStatusStr);


		ListBox_SetTopIndex(hwndListInfo, index);
		// now repaint all at once
		SetWindowRedraw(hwndListInfo, TRUE);
	}
}

void MainDlgSetStatus(HWND hDlg, const char *msg)
{
	SendDlgItemMessage(hDlg, IDC_STATIC_STATUS, WM_SETTEXT, 0, (LPARAM)msg);
}

void MainDlgUpdateComDropdown(HWND hDlg)
{
	XYZV3::refreshPortList();
	int count = XYZV3::getPortCount();

	if(count >= g_maxPorts)
		count = g_maxPorts;

	SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_RESETCONTENT, 0, 0);
	for(int i=0; i<= count; i++)
	{
		int port = -1;
		const char *name = "Auto";
		if(i > 0)
		{
			port = XYZV3::getPortNumber(i-1);
			name = XYZV3::getPortName(i-1);
		}

		g_comIDtoPort[i] = port;
		
		SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_ADDSTRING, 0, (LPARAM)name);
	}
	SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_SETCURSEL, 0, 0);
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
	if(!g_threadRunning && xyz.updateStatus())
	{
		const XYZPrinterState *st = xyz.getPrinterState();
		const XYZPrinterInfo *inf = xyz.getPrinterInfo();

		if(st->isValid)
		{
			MainDlgUpdateStatusList(hDlg, st, inf);

			SendDlgItemMessage(hDlg, IDC_CHECK_BUZZER, BM_SETCHECK, (WPARAM)(st->sBuzzerEnabled) ? BST_CHECKED : BST_UNCHECKED, 0);
			SendDlgItemMessage(hDlg, IDC_CHECK_AUTO, BM_SETCHECK, (WPARAM)(st->oAutoLevelEnabled) ? BST_CHECKED : BST_UNCHECKED, 0);

			SetDlgItemInt(hDlg, IDC_EDIT_ZOFF, st->zOffset, false);

			int pct = max(g_printPct, st->dPrintPercentComplete);
			SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_SETPOS, pct, 0);
		}
	}
	else
		SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_SETPOS, g_printPct, 0);
	debugReduceNoise(false);
}

void MainDlgConnect(HWND hDlg)
{
	int comID = SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_GETCURSEL, 0, 0);
	if(comID == CB_ERR)
		comID = 0;

	if(xyz.connect(g_comIDtoPort[comID]))
		MainDlgSetStatus(hDlg, "connected");
	else
		MainDlgSetStatus(hDlg, "not connected");
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

BOOL CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int t;
    switch(msg) 
    {
        case WM_INITDIALOG:
			defaultCursor = LoadCursor(NULL, IDC_ARROW);    // default cursor
			waitCursor = LoadCursor(NULL, IDC_WAIT);     // wait cursor

			MainDlgUpdateComDropdown(hDlg);
			MainDlgUpdateModelDropdown(hDlg);
			MainDlgConnect(hDlg);

			hwndListInfo = GetDlgItem(hDlg, IDC_LIST_STATUS);

			g_timer = SetTimer(hDlg, NULL, g_timerInterval, NULL);

			SendDlgItemMessage(hDlg, IDC_SPIN_ZOFF, UDM_SETRANGE, 0, MAKELONG( 1000, 1));

			// force a refresh, without waiting for timer
			MainDlgUpdate(hDlg);
            break;

        case WM_DESTROY:
            // Cleanup everything
			PostQuitMessage(0);
            break;

		case WM_CLOSE:
			DestroyWindow(hDlg);
			xyz.disconnect();

			KillTimer(hDlg, g_timer);
	        return TRUE;

        case WM_ACTIVATE:
            break;

		case WM_SETCURSOR:
			if(g_threadRunning)
				SetCursor(waitCursor);
			else
				SetCursor(defaultCursor);
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
			//****FixMe, how do I id what spin control triggered?
            if(LOWORD(wParam) == SB_ENDSCROLL)
				setZOffset(hDlg);
			break;

        case WM_COMMAND:

			if(g_threadRunning && LOWORD(wParam) != IDCANCEL)
				break;

            switch(LOWORD(wParam))
            {
			case IDOK:
				if(GetFocus() == GetDlgItem(hDlg, IDC_EDIT_ZOFF))
					setZOffset(hDlg);
				break;

            case IDCANCEL:
                EndDialog(hDlg, 0);
                break;

			case IDC_COMBO_PORT:
				if(HIWORD(wParam) == CBN_SELCHANGE)
					MainDlgConnect(hDlg);
				else if(HIWORD(wParam) == CBN_DROPDOWN)
					MainDlgUpdateComDropdown(hDlg);
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
				MainDlgSetStatus(hDlg, "loading fillament");
				if(xyz.loadFillamentStart())
				{
					MessageBox(NULL, "Hit ok when filliment comes out of nozzle.", "Load Fillament", MB_OK);
					if(xyz.loadFillamentFinish())
						MainDlgSetStatus(hDlg, "loading fillament complete");
					else
						MainDlgSetStatus(hDlg, "loading fillament failed");
				}
				else
					MainDlgSetStatus(hDlg, "loading fillament failed");
				SetCursor(defaultCursor);
				break;

			case IDC_BUTTON_UNLOAD: 
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "unloading fillament");
				if(xyz.unloadFillament())
					MainDlgSetStatus(hDlg, "unloading fillament complete");
				else
					MainDlgSetStatus(hDlg, "unloading fillament failed");
				SetCursor(defaultCursor);
				break;

			case IDC_BUTTON_CLEAN: 
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "cleaning nozzle");
				if(xyz.cleanNozzleStart())
				{
					MessageBox(NULL, "Hit ok after cleaning nozzle.", "Clean Nozzle", MB_OK);
					if(xyz.cleanNozzleFinish())
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
					if(xyz.calibrateBedRun())
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

			case IDC_EDIT_ZOFF:
				if(HIWORD(wParam) == EN_KILLFOCUS)
					setZOffset(hDlg);
				break;

			case IDC_BUTTON_HOME: 
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "homing printer");
				if(xyz.homePrinter()) 
					MainDlgSetStatus(hDlg, "homing printer complete");
				else
					MainDlgSetStatus(hDlg, "homing printer failed");
				SetCursor(defaultCursor);
				break;

			case IDC_BUTTON_XP: 
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "move x");
				if(xyz.jogPrinter('x', 10))
					MainDlgSetStatus(hDlg, "move x complete");
				else
					MainDlgSetStatus(hDlg, "move x failed");
				SetCursor(defaultCursor);
				break;

			case IDC_BUTTON_XM: 
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "move x");
				if(xyz.jogPrinter('x', -10))
					MainDlgSetStatus(hDlg, "move x complete");
				else
					MainDlgSetStatus(hDlg, "move x failed");
				SetCursor(defaultCursor);
				break;

			case IDC_BUTTON_YP: 
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "move y");
				if(xyz.jogPrinter('y', 10))
					MainDlgSetStatus(hDlg, "move y complete");
				else
					MainDlgSetStatus(hDlg, "move y failed");
				SetCursor(defaultCursor);
				break;

			case IDC_BUTTON_YM: 
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "move y");
				if(xyz.jogPrinter('y', -10))
					MainDlgSetStatus(hDlg, "move y complete");
				else
					MainDlgSetStatus(hDlg, "move y failed");
				SetCursor(defaultCursor);
				break;

			case IDC_BUTTON_ZP: 
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "move z");
				if(xyz.jogPrinter('z', 10))
					MainDlgSetStatus(hDlg, "move z complete");
				else
					MainDlgSetStatus(hDlg, "move z failed");
				SetCursor(defaultCursor);
				break;

			case IDC_BUTTON_ZM: 
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "move z");
				if(xyz.jogPrinter('z', -10))
					MainDlgSetStatus(hDlg, "move z complete");
				else
					MainDlgSetStatus(hDlg, "move z failed");
				SetCursor(defaultCursor);
				break;

			case IDC_CHECK_AUTO:
				SetCursor(waitCursor);
				MainDlgSetStatus(hDlg, "toggle auto level");
                if (SendDlgItemMessage(hDlg, IDC_CHECK_AUTO, BM_GETCHECK, 0, 0)) 
					xyz.enableAutoLevel(true);
				else
					xyz.enableAutoLevel(false);
				SetCursor(defaultCursor);
				break;

			case IDC_CHECK_BUZZER:
				MainDlgSetStatus(hDlg, "toggle buzzer");
				SetCursor(waitCursor);
                if (SendDlgItemMessage(hDlg, IDC_CHECK_BUZZER, BM_GETCHECK, 0, 0)) 
					xyz.enableBuzzer(true);
				else
					xyz.enableBuzzer(false);
				SetCursor(defaultCursor);
				break;

			//xyz.restoreDefaults();
			//xyz.setLanguage(bgStr1);

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
