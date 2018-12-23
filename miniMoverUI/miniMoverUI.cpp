
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
#include "stream.h"
#include "serial.h"
#include "socket.h"
#include "xyzv3.h"
#include "XYZPrinterList.h"

// tell windows to give us a more modern look
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma warning(disable: 4996) // disable deprecated warning 
#pragma warning(disable: 4800)

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Comctl32.lib")

//-------------------------------------------
// globals

// uncomment to reduce amount of 'routine' logging in debug log
//#define DEBUG_REDUCE_NOISE

HINSTANCE g_hInst = NULL;

XYZV3 xyz;
Serial g_serial;
Socket g_soc;

WifiList g_wifiList;
int g_listWifiOffset = -1;
int g_listSerialOffset = -1;
XYZPrinterStatus g_prSt = { 0 };

UINT_PTR g_main_timer = 0;
bool g_block_update = false;


// set by XYZV3Thread.cpp
//int g_printPct = 0;

bool g_wifiOptionsEdited = false;

// controls
HWND hwndListInfo = NULL;

HCURSOR waitCursor;
HCURSOR defaultCursor;

void MainDlgSetStatus(HWND hDlg, const char *msg);

//-------------------------------------------
// helper 'run' dialog

enum ActionCommand
{
	ACT_IDLE,
	ACT_FINISH_WAIT,

	ACT_CALIB_BED,
	ACT_CLEAN_NOZZLE,
	ACT_HOME_PRINTER,
	ACT_JOG_PRINTER,
	ACT_LOAD_FILLAMENT,
	ACT_UNLOAD_FILLAMENT,
	ACT_PRINT_FILE,
	ACT_PRINT_FILE_MONITOR,
	ACT_CONVERT_FILE,
	ACT_UPLOAD_FIRMWARE,
	ACT_SET_ZOFFSET,
	ACT_SET_MACHINE_NAME,
	ACT_DISCONNECT_WIFI,
	ACT_SET_WIFI,
	ACT_SET_ENERGY,
	ACT_SET_LANGUAGE,
	ACT_SET_AUTO_LEVEL,
	ACT_SET_BUZZER,
};

ActionCommand g_run_act = ACT_IDLE;
UINT_PTR g_run_timer = 0;
bool g_run_doPause = true;

int g_run_i1 = -1;
int g_run_i2 = -1;
char g_run_str1[MAX_PATH];
char g_run_str2[MAX_PATH];

void runSetButton(HWND hDlg, int num, bool enable, const char *str = NULL)
{
	assert(num >= 0 && num <=3);
	assert(!enable || str);

	int id = -1;
	if(num == 0)
		id = IDCANCEL;
	else if(num == 1)
		id = ID_RUN_BTN1;
	else if(num == 2)
		id = ID_RUN_BTN2;

	if(id >= 0)
	{
		ShowWindow(GetDlgItem(hDlg, id), (enable) ? SW_SHOW : SW_HIDE);

		if(enable && str)
			SetDlgItemText(hDlg, id, str);
	}
}

// progress of -1 is 'buisy' marquee
void runSetProgressBar(HWND hDlg, int progress)
{
	LONG_PTR style = GetWindowLongPtr(GetDlgItem(hDlg, IDC_RUN_PROGRESS), GWL_STYLE);
	if(progress < 0)
	{
		if(!(style & PBS_MARQUEE))
			SetWindowLongPtr(GetDlgItem(hDlg, IDC_RUN_PROGRESS), GWL_STYLE, style | PBS_MARQUEE);
		SendMessage(GetDlgItem(hDlg, IDC_RUN_PROGRESS), (UINT)PBM_SETMARQUEE, (WPARAM)1, (LPARAM)NULL);
	}
	else
	{
		if((style & PBS_MARQUEE))
			SetWindowLongPtr(GetDlgItem(hDlg, IDC_RUN_PROGRESS), GWL_STYLE, style & ~PBS_MARQUEE);
		SendDlgItemMessage(hDlg, IDC_RUN_PROGRESS, PBM_SETPOS, progress, 0);
	}
}

void runDoInit(HWND hDlg)
{
	//SetWindowText(hDlg, "whatever");
	SetDlgItemText(hDlg, IDC_RUN_STATIC2, "initializing");
	runSetProgressBar(hDlg, 0);

	runSetButton(hDlg, 2, false);
	runSetButton(hDlg, 1, false);
	runSetButton(hDlg, 0, false);

	switch(g_run_act)
	{
	case ACT_CALIB_BED:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Calibrating Bed");
		xyz.calibrateBedStart();
		break;
	case ACT_CLEAN_NOZZLE:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Clean Nozzle");
		xyz.cleanNozzleStart();
		runSetButton(hDlg, 0, true, "Cancel");
		break;
	case ACT_HOME_PRINTER:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Home Printer");
		xyz.homePrinterStart();
		break;
	case ACT_JOG_PRINTER:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Jog Printer");
		xyz.jogPrinterStart(g_run_i1, g_run_i2);
		break;
	case ACT_LOAD_FILLAMENT:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Loading Fillament");
		xyz.loadFilamentStart();
		runSetButton(hDlg, 0, true, "Cancel");
		break;
	case ACT_UNLOAD_FILLAMENT:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Unloading Fillament");
		xyz.unloadFilamentStart();
		runSetButton(hDlg, 0, true, "Cancel");
		break;
	case ACT_PRINT_FILE:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Printing File");
		xyz.printFileStart(g_run_str1);
		break;
	case ACT_PRINT_FILE_MONITOR:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Monitoring Print");
		xyz.queryStatusStart();
		g_run_doPause = true;
		runSetButton(hDlg, 2, true, "Cancel");
		runSetButton(hDlg, 1, true, "Pause");
		runSetButton(hDlg, 0, true, "OK");
		break;
	case ACT_CONVERT_FILE:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Converting File");
		xyz.convertFileStart(g_run_str1, g_run_str2, g_run_i1);
		break;
	case ACT_UPLOAD_FIRMWARE:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Uploading Firmware");
		xyz.uploadFirmwareStart(g_run_str1);
		break;
	case ACT_SET_ZOFFSET:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Set Z-Offset");
		xyz.setZOffsetStart(g_run_i1);
		break;
	case ACT_SET_MACHINE_NAME:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Set Machine Name");
		xyz.setMachineNameStart(g_run_str1);
		break;
	case ACT_DISCONNECT_WIFI:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Disconnect WiFi");
		xyz.sendDisconnectWifiStart();
		break;
	case ACT_SET_WIFI:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Setup WiFi");
		xyz.setWifiStart(g_run_str1, g_run_str2, g_run_i1);
		break;
	case ACT_SET_ENERGY:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Set Energy Savings");
		xyz.setEnergySavingStart(g_run_i1);
		break;
	case ACT_SET_LANGUAGE:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Set Language");
		xyz.setLanguageStart(g_run_str1);
		break;
	case ACT_SET_AUTO_LEVEL:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Set Auto Level");
		xyz.setAutoLevelStart(g_run_i1);
		break;
	case ACT_SET_BUZZER:
		SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Set Buzzer");
		xyz.setBuzzerStart(g_run_i1);
		break;
	default:
		assert(false);
		break;
	}
}

bool runDoProcess(HWND hDlg)
{
	xyz.doProcess();

	if(g_run_act != ACT_PRINT_FILE_MONITOR)
		runSetProgressBar(hDlg, xyz.getProgressPct());

	switch(g_run_act)
	{
	case ACT_IDLE:
		return false;	// time to quit
	case ACT_FINISH_WAIT:
		// spin till we are done
		return true;
	case ACT_CALIB_BED:
		if(!xyz.isInProgress()) {
			runSetButton(hDlg, 0, true, "OK");
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Calibrating Bed finished, hit ok to exit");
			g_run_act = ACT_FINISH_WAIT;
		} else if(xyz.calibrateBedPromptToLowerDetector()) {
			runSetButton(hDlg, 0, true, "OK");
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Lower calibration sensor and hit ok");
		} else if(xyz.calibrateBedPromptToRaiseDetector()) {
			runSetButton(hDlg, 0, true, "OK");
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Raise calibration sensor and hit ok");
		} else {
			runSetButton(hDlg, 0, false);
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, xyz.getStateStr());
		}
		return true;
	case ACT_CLEAN_NOZZLE:
		if(!xyz.isInProgress()) {
			runSetButton(hDlg, 0, true, "OK");
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Clean nozzle finished, hit ok to exit");
			g_run_act = ACT_FINISH_WAIT;
		} else if(xyz.cleanNozzlePromtToClean()) {
			runSetButton(hDlg, 0, true, "OK");
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Clean nozzle with wire and hit ok when finished");
		}
		else
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, xyz.getStateStr());
		return true;
	case ACT_LOAD_FILLAMENT:
		if(!xyz.isInProgress()) {
			runSetButton(hDlg, 0, true, "OK");
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Load fillament finished, hit ok to exit");
			g_run_act = ACT_FINISH_WAIT;
		} else if(xyz.loadFilamentPromptToFinish()) {
			runSetButton(hDlg, 0, true, "OK");
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Wait for fillament to come out of nozzle then hit ok");
		} else
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, xyz.getStateStr());
		return true;
	case ACT_PRINT_FILE:
		if(!xyz.isInProgress())
		{
			SetDlgItemText(hDlg, IDC_RUN_STATIC1, "Monitoring Print");
			g_run_doPause = true;
			runSetButton(hDlg, 2, true, "Cancel");
			runSetButton(hDlg, 1, true, "Pause");
			runSetButton(hDlg, 0, true, "OK");
			g_run_act = ACT_PRINT_FILE_MONITOR;
		}
		else
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, xyz.getStateStr());
		return true;
	case ACT_PRINT_FILE_MONITOR:
		//****FixMe, query status should just happen
		if(g_run_doPause)
			runSetButton(hDlg, 1, true, "Pause");
		else
			runSetButton(hDlg, 1, true, "Resume");

		if(!xyz.isInProgress())
		{
			const XYZPrinterStatus *st = xyz.getPrinterStatus();
			if(st->isValid)
			{
				runSetProgressBar(hDlg, st->dPrintPercentComplete);

				char tstr[1024] = "";

				if(st->eErrorStatus != 0)
					sprintf(tstr, "Status: %s, error: (0x%08x)%s", st->jPrinterStateStr, st->eErrorStatus, st->eErrorStatusStr);
				else
				{
					sprintf(tstr, "Status: %s", st->jPrinterStateStr);

					if(st->tExtruderCount > 1 && st->tExtruder2ActualTemp_C > 0)
						sprintf(tstr+strlen(tstr), ", temp: %d C %d C / %d C", st->tExtruder1ActualTemp_C, st->tExtruder2ActualTemp_C, st->tExtruderTargetTemp_C);
					else
						sprintf(tstr+strlen(tstr), ", temp: %d C / %d C", st->tExtruder1ActualTemp_C, st->tExtruderTargetTemp_C);

					if(st->bBedActualTemp_C > 30)
						sprintf(tstr+strlen(tstr), ", bed: %d C / %d C", st->bBedActualTemp_C, st->OBedTargetTemp_C);

					if(st->dPrintPercentComplete != 0 || st->dPrintElapsedTime_m != 0 || st->dPrintTimeLeft_m != 0)
						sprintf(tstr+strlen(tstr), ", time: %d/%d min", st->dPrintElapsedTime_m, st->dPrintTimeLeft_m);
				}

				SetDlgItemText(hDlg, IDC_RUN_STATIC2, tstr);

				if( st->jPrinterState == PRINT_ENDING_PROCESS_DONE ||
					st->jPrinterState == PRINT_NONE )
				{
					runSetButton(hDlg, 0, true, "OK");
					runSetButton(hDlg, 1, false);
					runSetButton(hDlg, 2, false);
					SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Print job finished, hit ok to exit");
					g_run_act = ACT_FINISH_WAIT;
				}
			}

			// query again
			xyz.queryStatusStart();
		}
		return true;

	// generic case
	case ACT_HOME_PRINTER:
	case ACT_UNLOAD_FILLAMENT:
	case ACT_CONVERT_FILE:
	case ACT_UPLOAD_FIRMWARE:
		if(!xyz.isInProgress())
		{
			runSetButton(hDlg, 0, true, "OK");
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Process finished, hit ok to exit");
			g_run_act = ACT_FINISH_WAIT;
		}
		else
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, xyz.getStateStr());
		return true;

	// quit when finished
	case ACT_JOG_PRINTER:
	case ACT_SET_ZOFFSET:
	case ACT_SET_MACHINE_NAME:
	case ACT_DISCONNECT_WIFI:
	case ACT_SET_WIFI:
	case ACT_SET_ENERGY:
	case ACT_SET_LANGUAGE:
	case ACT_SET_AUTO_LEVEL:
	case ACT_SET_BUZZER:
		if(!xyz.isInProgress())
		{
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Process finished");
			g_run_act = ACT_IDLE;
		}
		else
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, xyz.getStateStr());
		return true;


	}

	// if we got to here then something is wrong, just bail
	return false;
}

// btn 0 == cancel
// btn 1 == button 1
// btn 2 == button 2
void runDoAction(HWND hDlg, int btn)
{
	switch(g_run_act)
	{
	case ACT_FINISH_WAIT:
		g_run_act = ACT_IDLE; // set state to exit
		break;
	case ACT_CALIB_BED:
		if(xyz.calibrateBedPromptToLowerDetector())
			xyz.calibrateBedDetectorLowered();
		else if(xyz.calibrateBedPromptToRaiseDetector())
			xyz.calibrateBedDetectorRaised();
		break;
	case ACT_CLEAN_NOZZLE:
		xyz.cleanNozzleCancel();
		break;
	case ACT_LOAD_FILLAMENT:
		xyz.loadFilamentCancel();
		break;
	case ACT_UNLOAD_FILLAMENT:
		xyz.unloadFilamentCancel();
		break;
	case ACT_PRINT_FILE_MONITOR:
		if(btn == 0)
			g_run_act = ACT_IDLE;
		else if(btn == 1)
		{
			if(g_run_doPause)
				xyz.pausePrint();
			else
				xyz.resumePrint();
			g_run_doPause = !g_run_doPause;
		}
		else if(btn == 2)
		{
			xyz.cancelPrint();
			SetDlgItemText(hDlg, IDC_RUN_STATIC2, "Print job canceled, hit ok to exit");
			runSetButton(hDlg, 0, true, "OK");
			runSetButton(hDlg, 1, false);
			runSetButton(hDlg, 2, false);
			g_run_act = ACT_FINISH_WAIT;
		}
		break;
	case ACT_IDLE:
	case ACT_HOME_PRINTER:
	case ACT_JOG_PRINTER:
	case ACT_PRINT_FILE:
	case ACT_CONVERT_FILE:
	case ACT_UPLOAD_FIRMWARE:
	case ACT_SET_ZOFFSET:
	case ACT_SET_MACHINE_NAME:
	case ACT_DISCONNECT_WIFI:
	case ACT_SET_WIFI:
	case ACT_SET_ENERGY:
	case ACT_SET_LANGUAGE:
	case ACT_SET_AUTO_LEVEL:
	case ACT_SET_BUZZER:
		break;
	}
}

BOOL CALLBACK RunDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    switch (message) 
    { 
		case WM_INITDIALOG:
			runDoInit(hDlg);
			// set a timer to trigger basically immediatly
			g_run_timer = SetTimer(hDlg, NULL, 50, NULL);
            return TRUE; 

		case WM_TIMER:
			if(!runDoProcess(hDlg))
			{
				KillTimer(hDlg, g_run_timer);
			    EndDialog(hDlg, 0); 
			}
            return TRUE; 

        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
			case IDOK:
            case IDCANCEL: 
				runDoAction(hDlg, 0);
				return TRUE;
            case ID_RUN_BTN1: 
				runDoAction(hDlg, 1);
				return TRUE;
            case ID_RUN_BTN2: 
				runDoAction(hDlg, 2);
				return TRUE;
            } 
    } 

	// we did not handle this message
    return FALSE; 
} 

void RunDialogStart(HWND hDlg, ActionCommand act, const char *status, int i1 = -1, int i2 = -1)
{
	assert(status);

	char tstr[512] = "";
	MainDlgSetStatus(hDlg, (status) ? status : tstr);

	g_block_update = true;
	g_run_act = act;
	g_run_i1 = i1;
	g_run_i2 = i2;
	DialogBox(g_hInst, MAKEINTRESOURCE(IDD_RUN_DIALOG), hDlg, RunDialogProc);
	g_run_timer = SetTimer(hDlg, g_run_timer, 50, NULL);
	g_block_update = false;

	if(status)
		sprintf(tstr, "%s complete", status);
	MainDlgSetStatus(hDlg, tstr);
}


//-------------------------------------------
// main dialog

bool getFilePath(HWND hDlg, char *path, int len, bool isOpen)
{
	if(path && len > 0)
	{
		OPENFILENAME ofn;
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = hDlg;
		//ofn.lpstrFilter = "Print Files\0*.gcode;*.3w\0GCode Files\0*.gcode\03W Files\0*.3w\0All Files\0*.*\0";
		ofn.lpstrFilter = "All Files\0*.*\0GCode Files\0*.gcode\0XYZ Files\0*.3w\0\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = path;
		if(isOpen)
			ofn.lpstrFile[0] = '\0'; // zero string if selecting a file
		ofn.nMaxFile = len;
		ofn.Flags = OFN_PATHMUSTEXIST | ((isOpen) ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT);

		if(isOpen)
			return (TRUE == GetOpenFileName(&ofn) && strlen(path) > 0);
		else
			return (TRUE == GetSaveFileName(&ofn) && strlen(path) > 0);
	}

	return false;
}

void setDialogStr(HWND hDlg, const char *format, ...)
{
	const static int BUFF_SIZE = 2048;
	char msgBuf[BUFF_SIZE];
	va_list arglist;

	va_start(arglist, format);
	vsnprintf(msgBuf, sizeof(msgBuf), format, arglist);
	msgBuf[sizeof(msgBuf)-1] = '\0';
	va_end(arglist);

	SetWindowText(hDlg, msgBuf);
}

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
		listAddLine(hwndListInfo, "Z Offset: %0.2f mm", st->zOffset/100.0f);
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

		if(*st->lLang)
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

		bool isPrinting = st->jPrinterState >= PRINT_INITIAL && st->jPrinterState <= PRINT_JOB_DONE;
		if(isPrinting)
			SendDlgItemMessage(hDlg, IDC_BUTTON_PRINT, WM_SETTEXT, 0, (LPARAM)"Monitor Print");
		else
			SendDlgItemMessage(hDlg, IDC_BUTTON_PRINT, WM_SETTEXT, 0, (LPARAM)"Print File");

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
	static char oldMsg[512] = "";
	if(0 != strcmp(oldMsg, msg))
		debugPrint(DBG_LOG, "MainDlgSetStatus(%s)", msg);
	strcpy(oldMsg, msg);
	SendDlgItemMessage(hDlg, IDC_STATIC_STATUS, WM_SETTEXT, 0, (LPARAM)msg);
}

int getMoveDist(HWND hDlg)
{
	int t = SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_GETCURSEL, 0, 0);
	if(t == 0) return 1;
	if(t == 1) return 10;
	if(t == 2) return 100;
	if(t == 3) return 300;
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
		// look for screen name
		const char *name = XYZV3::serialToName(g_wifiList.m_list[i].m_serialNum);
		//sprintf(tstr, "%s (%s:%d)", name, g_wifiList.m_list[i].m_ip, g_wifiList.m_list[i].m_port);
		sprintf(tstr, "%s (%s)", name, g_wifiList.m_list[i].m_ip);
		SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_ADDSTRING, 0, (LPARAM)tstr);
		ent++;
	}

	// add in serial printers
	g_listSerialOffset = ent;
	SerialHelper::queryForPorts("XYZ");
	int count = SerialHelper::getPortCount();
	for(int i=0; i< count; i++)
	{
		const char *port = SerialHelper::getPortDisplayName(i);
		SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_ADDSTRING, 0, (LPARAM)port);
		ent++;
	}
}

void MainDlgUpdateModelDropdown(HWND hDlg)
{
	SendDlgItemMessage(hDlg, IDC_COMBO_MODEL, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage(hDlg, IDC_COMBO_MODEL, CB_ADDSTRING, 0, (LPARAM)"Auto");
	for(int i=0; i < XYZV3::getInfoCount(); i++)
	{
		SendDlgItemMessage(hDlg, IDC_COMBO_MODEL, CB_ADDSTRING, 0, (LPARAM)XYZV3::indexToInfo(i)->screenName);
	}
	SendDlgItemMessage(hDlg, IDC_COMBO_MODEL, CB_SETCURSEL, 0, 0);
}

void setZOffset(HWND hDlg, int zOffset)
{
	char tstr[32];
	sprintf(tstr, "%d.%02d", zOffset / 100, zOffset % 100);
	SetDlgItemTextA(hDlg, IDC_EDIT_ZOFF, tstr);
}

int getZOffset(HWND hDlg)
{
	char tstr[32];
	if(GetDlgItemTextA(hDlg, IDC_EDIT_ZOFF, tstr, 32))
	{
		int a = 0, b = 0;
		sscanf(tstr, "%d.%d", &a, &b);
		return a * 100 + b;
	}
	return -1;
}

void MainDlgUpdate(HWND hDlg)
{
#ifdef DEBUG_REDUCE_NOISE
	debugReduceNoise(true);
#endif

	// skip update if requested to 
	if(g_block_update)
		return;

	debugPrint(DBG_VERBOSE, "MainDlgUpdate()");

	// don't set wait cursor since this triggers 2x a second
	if(xyz.isStreamSet())
	{
		xyz.doProcess();
		if(!xyz.isInProgress())
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
					setZOffset(hDlg, st->zOffset);

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

				//int pct = max(g_printPct, st->dPrintPercentComplete);
				//SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_SETPOS, pct, 0);

				if(0 != strcmp(g_prSt.lLang, st->lLang))
				{
					int id = 0;
					for(id=0; id<XYZPrintingLangCount; id++)
					{
						if(0 == strcmp(st->lLang, XYZPrintingLang[id].abrv))
							break;
					}

					// default to en if not found
					if(id == XYZPrintingLangCount)
						id = 0;

					SendDlgItemMessage(hDlg, IDC_COMBO_LANGUAGE, CB_SETCURSEL, id, 0);
				}

				// if network info changed
				if(0 != strcmp(g_prSt.iMachineSerialNum, st->iMachineSerialNum) ||
				   0 != strcmp(g_prSt.N4NetIP, st->N4NetIP) )
				{
					// if contains wifi info
					if(st->N4NetIP[0])
					{
						// find entry, adding new if not found
						WifiEntry *ent = g_wifiList.findEntry(st->iMachineSerialNum, true);
						if(ent)
							ent->set(st->iMachineSerialNum, st->N4NetIP);
					}
				}

				// copy to backup
				memcpy(&g_prSt, st, sizeof(g_prSt));
			}
			xyz.queryStatusStart();
		}
		//else SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_SETPOS, g_printPct, 0);
	}
	else MainDlgSetStatus(hDlg, "not connected");

#ifdef DEBUG_REDUCE_NOISE
	debugReduceNoise(false);
#endif
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
	xyz.setStream(NULL);
	// clear out old status
	SendMessage(hwndListInfo, LB_RESETCONTENT, 0, 0);
	// and notify user
	MainDlgSetStatus(hDlg, "not connected");
	setDialogStr(hDlg, "MiniMoverUI %s - %s", g_ver, "not connected");

	// if auto detect
	if(comID == 0)
	{
		// try serial first
		int id = SerialHelper::queryForPorts("XYZ");
		if(id >= 0 && g_serial.openStream(SerialHelper::getPortDeviceName(id), 115200))
		{
			xyz.setStream(&g_serial);
			MainDlgSetStatus(hDlg, "connected");
			setDialogStr(hDlg, "MiniMoverUI %s - %s", g_ver, SerialHelper::getPortDisplayName(id));
		}
		// fall back to wifi if no serial
		else if(g_wifiList.m_count > 0)
		{
			if(g_soc.openStream(g_wifiList.m_list[0].m_ip, g_wifiList.m_list[0].m_port))
			{
				xyz.setStream(&g_soc);
				MainDlgSetStatus(hDlg, "connected");
				const char *name = XYZV3::serialToName(g_wifiList.m_list[0].m_serialNum);
				setDialogStr(hDlg, "MiniMoverUI %s - %s (%s:%d)", g_ver, name, g_wifiList.m_list[0].m_ip, g_wifiList.m_list[0].m_port);
			}
		}
	}
	// else if serial port
	else if(comID >= g_listSerialOffset)
	{
		int id = comID - g_listSerialOffset;
		if(id >= 0 && g_serial.openStream(SerialHelper::getPortDeviceName(id), 115200))
		{
			xyz.setStream(&g_serial);
			MainDlgSetStatus(hDlg, "connected");
			setDialogStr(hDlg, "MiniMoverUI %s - %s", g_ver, SerialHelper::getPortDisplayName(id));
		}
	}
	// else if wifi
	else if(comID >= g_listWifiOffset && comID < g_listSerialOffset)
	{
		int id = comID - g_listWifiOffset;
		if(id >=0 && id < g_wifiList.m_count)
		{
			if(g_soc.openStream(g_wifiList.m_list[id].m_ip, g_wifiList.m_list[id].m_port))
			{
				xyz.setStream(&g_soc);

				MainDlgSetStatus(hDlg, "connected");
				const char *name = XYZV3::serialToName(g_wifiList.m_list[id].m_serialNum);
				setDialogStr(hDlg, "MiniMoverUI %s - %s (%s:%d)", g_ver, name, g_wifiList.m_list[id].m_ip, g_wifiList.m_list[id].m_port);
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
		{
			SendDlgItemMessage(hDlg, IDC_COMBO_LANGUAGE, CB_ADDSTRING, 0, (LPARAM)XYZPrintingLang[i].desc);
		}
		SendDlgItemMessage(hDlg, IDC_COMBO_LANGUAGE, CB_SETCURSEL, 0, 0); // default to something

		SendDlgItemMessage(hDlg, IDC_COMBO_ENERGY_SAVING, CB_ADDSTRING, 0, (LPARAM)"off");
		SendDlgItemMessage(hDlg, IDC_COMBO_ENERGY_SAVING, CB_ADDSTRING, 0, (LPARAM)"3 min");
		SendDlgItemMessage(hDlg, IDC_COMBO_ENERGY_SAVING, CB_ADDSTRING, 0, (LPARAM)"6 min");
		SendDlgItemMessage(hDlg, IDC_COMBO_ENERGY_SAVING, CB_SETCURSEL, 1, 0); // default to something

		SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_ADDSTRING, 0, (LPARAM)"1 mm");
		SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_ADDSTRING, 0, (LPARAM)"10 mm");
		SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_ADDSTRING, 0, (LPARAM)"100 mm");
		SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_ADDSTRING, 0, (LPARAM)"300 mm");
		SendDlgItemMessage(hDlg, IDC_COMBO_DIST, CB_SETCURSEL, 1, 0); // default to something

		hwndListInfo = GetDlgItem(hDlg, IDC_LIST_STATUS);

		g_block_update = false;

		SendDlgItemMessage(hDlg, IDC_SPIN_ZOFF, UDM_SETRANGE, 0, MAKELONG( 1000, 1));
		SendDlgItemMessage(hDlg, IDC_SPIN_WIFI_CHAN, UDM_SETRANGE, 0, MAKELONG( 14, 1));

		xyz.queryStatusStart();
		g_main_timer = SetTimer(hDlg, g_main_timer, 50, NULL);

		// force a refresh, without waiting for timer
		//MainDlgUpdate(hDlg);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_CLOSE:
		// Cleanup everything
		DestroyWindow(hDlg);
		g_wifiList.writeWifiList();
		KillTimer(hDlg, g_main_timer);
		xyz.setStream(NULL);
		return TRUE;

	case WM_ACTIVATE:
		break;

	case  WM_TIMER:
		MainDlgUpdate(hDlg);
		break;

	case WM_NOTIFY:
		{
			LPNMUPDOWN lpnmud = (LPNMUPDOWN) lParam;
			int zOffset;

			switch(LOWORD(wParam))
			{
			case IDC_SPIN_ZOFF:
				zOffset = getZOffset(hDlg);
				if(zOffset >= 0)
				{
					zOffset += lpnmud->iDelta;
					setZOffset(hDlg, zOffset);
					RunDialogStart(hDlg, ACT_SET_ZOFFSET, "set z-offset", zOffset);
				}
				break;
			}
		}
		break;

		/*
	case  WM_VSCROLL:
		if(LOWORD(wParam) == SB_ENDSCROLL)
		{
			if((HWND)lParam ==  GetDlgItem(hDlg, IDC_SPIN_ZOFF)) 
			{
				int zOffset = getZOffset(hDlg);
				if(zOffset >= 0)
					RunDialogStart(hDlg, ACT_SET_ZOFFSET, "set z-offset", zOffset);
			}
		}
		break;
		*/

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			if(GetFocus() == GetDlgItem(hDlg, IDC_EDIT_ZOFF))
			{
				int zOffset = getZOffset(hDlg);
				if(zOffset >= 0)
					RunDialogStart(hDlg, ACT_SET_ZOFFSET, "set z-offset", zOffset);
			}
			else if(GetFocus() == GetDlgItem(hDlg, IDC_EDIT_MACHINE_NAME))
			{
				GetDlgItemTextA(hDlg, IDC_EDIT_MACHINE_NAME, g_run_str1, sizeof(g_run_str1));
				RunDialogStart(hDlg, ACT_SET_MACHINE_NAME, "set machine name");
			}
			break;

		case IDCANCEL:
			EndDialog(hDlg, 0);
			break;

		case IDC_BUTTON_PRINT:
			{
				const XYZPrinterStatus *st = xyz.getPrinterStatus();
				bool isPrinting = st->jPrinterState >= PRINT_INITIAL && st->jPrinterState <= PRINT_JOB_DONE;
				if(st->isValid && isPrinting)
					RunDialogStart(hDlg, ACT_PRINT_FILE_MONITOR, "Monitoring print");
				else
				{
					MainDlgSetStatus(hDlg, "printing file");
					if(getFilePath(hDlg, g_run_str1, sizeof(g_run_str1), true))
						RunDialogStart(hDlg, ACT_PRINT_FILE, "printing file");
					else MainDlgSetStatus(hDlg, "printing file failed to open file");
				}
			}
			break;

		case IDC_BUTTON_CONVERT:
			MainDlgSetStatus(hDlg, "converting file");
			t = SendDlgItemMessage(hDlg, IDC_COMBO_MODEL, CB_GETCURSEL, 0, 0)-1;
			if(getFilePath(hDlg, g_run_str1, sizeof(g_run_str1), true))
			{
				// fix up path
				strcpy(g_run_str2, g_run_str1);
				char *s = strrchr(g_run_str2, '.');
				if(s)
				{
					if(s[1] == 'g')
						sprintf(s, ".3w");
					else if(s[1] == '3')
						sprintf(s, ".gcode");

					if(getFilePath(hDlg, g_run_str2, sizeof(g_run_str2), false))
						RunDialogStart(hDlg, ACT_CONVERT_FILE, "converting file", t);
					else MainDlgSetStatus(hDlg, "converting failed to open destination file");
				}
				else MainDlgSetStatus(hDlg, "converting failed to open destination file");
			}
			else MainDlgSetStatus(hDlg, "converting failed to open source file");
			break;

		case IDC_BUTTON_LOAD:
			RunDialogStart(hDlg, ACT_LOAD_FILLAMENT, "loading filament");
			break;

		case IDC_BUTTON_UNLOAD: 
			RunDialogStart(hDlg, ACT_UNLOAD_FILLAMENT, "unloading filament");
			break;

		case IDC_BUTTON_CLEAN: 
			RunDialogStart(hDlg, ACT_CLEAN_NOZZLE, "cleaning nozzle");
			break;

		case IDC_BUTTON_CALIB: 
			RunDialogStart(hDlg, ACT_CALIB_BED, "calibrating bed");
			break;

		case IDC_BUTTON_HOME: 
			RunDialogStart(hDlg, ACT_HOME_PRINTER, "homing printer");
			break;

		case IDC_BUTTON_XP: 
			RunDialogStart(hDlg, ACT_JOG_PRINTER, "move x", 'x', getMoveDist(hDlg));
			break;

		case IDC_BUTTON_XM: 
			RunDialogStart(hDlg, ACT_JOG_PRINTER, "move x", 'x', -getMoveDist(hDlg));
			break;

		case IDC_BUTTON_YP: 
			RunDialogStart(hDlg, ACT_JOG_PRINTER, "move y", 'y', getMoveDist(hDlg));
			break;

		case IDC_BUTTON_YM: 
			RunDialogStart(hDlg, ACT_JOG_PRINTER, "move y", 'y', -getMoveDist(hDlg));
			break;

		case IDC_BUTTON_ZP: 
			RunDialogStart(hDlg, ACT_JOG_PRINTER, "move z", 'z', getMoveDist(hDlg));
			break;

		case IDC_BUTTON_ZM: 
			RunDialogStart(hDlg, ACT_JOG_PRINTER, "move z", 'z', -getMoveDist(hDlg));
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
						MainDlgSetStatus(hDlg, "auto detect wifi succeeded");
					}
					else
					{
						MainDlgSetStatus(hDlg, "failed to detect wifi password");
						SetDlgItemTextA(hDlg, IDC_EDIT_WIFI_PASSWD, "");
					}

					g_wifiOptionsEdited = true;
				}
				else MainDlgSetStatus(hDlg, "failed to detect wifi network");

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
				GetDlgItemTextA(hDlg, IDC_EDIT_WIFI_SSID, g_run_str1, sizeof(g_run_str1));
				GetDlgItemTextA(hDlg, IDC_EDIT_WIFI_PASSWD, g_run_str2, sizeof(g_run_str2));
				int chan = GetDlgItemInt(hDlg, IDC_EDIT_WIFI_CHAN, NULL, false);

				bool noPass = (0 == strcmp(g_run_str2, "******"));
				if(!*g_run_str1 || !*g_run_str2 || noPass || chan < 0)
				{
					MessageBox(NULL, "Invalid wifi settings.", "miniMover", MB_OK);
					MainDlgSetStatus(hDlg, "set wifi parameters failed");
				}
				else 
				{
					RunDialogStart(hDlg, ACT_SET_WIFI, "setup WiFi", chan);
					g_wifiOptionsEdited = false;
				}
			}
			break;

		case IDC_BUTTON_WIFI_DISCONNECT:
			RunDialogStart(hDlg, ACT_DISCONNECT_WIFI, "disconnect from WiFi");
			break;

		case IDC_EDIT_ZOFF:
			if(HIWORD(wParam) == EN_KILLFOCUS)
			{
				int zOffset = getZOffset(hDlg);
				if(zOffset >= 0)
					RunDialogStart(hDlg, ACT_SET_ZOFFSET, "set z-offset", zOffset);
			}
			break;

		case IDC_EDIT_MACHINE_NAME:
			if(HIWORD(wParam) == EN_KILLFOCUS)
			{
				GetDlgItemTextA(hDlg, IDC_EDIT_MACHINE_NAME, g_run_str1, sizeof(g_run_str1));
				RunDialogStart(hDlg, ACT_SET_MACHINE_NAME, "set machine name");
			}
			break;

		case IDC_COMBO_ENERGY_SAVING:
			if(HIWORD(wParam) == CBN_SELCHANGE)
			{
				int id = SendDlgItemMessage(hDlg, IDC_COMBO_LANGUAGE, CB_GETCURSEL, 0, 0);
				RunDialogStart(hDlg, ACT_SET_ENERGY, "set energy saving", id*3);
			}
			break;

		case IDC_COMBO_LANGUAGE:
			if(HIWORD(wParam) == CBN_SELCHANGE)
			{
				int id = SendDlgItemMessage(hDlg, IDC_COMBO_LANGUAGE, CB_GETCURSEL, 0, 0);
				strcpy(g_run_str1, XYZPrintingLang[id].abrv);
				RunDialogStart(hDlg, ACT_SET_LANGUAGE, "set language");
			}
			break;

		case IDC_COMBO_PORT:
			if(HIWORD(wParam) == CBN_SELCHANGE)
				MainDlgConnect(hDlg);
			else if(HIWORD(wParam) == CBN_DROPDOWN)
				MainDlgUpdateComDropdown(hDlg);
			break;

		case IDC_CHECK_AUTO:
			if (SendDlgItemMessage(hDlg, IDC_CHECK_AUTO, BM_GETCHECK, 0, 0)) 
				RunDialogStart(hDlg, ACT_SET_AUTO_LEVEL, "set auto level", true);
			else
				RunDialogStart(hDlg, ACT_SET_AUTO_LEVEL, "set auto level", false);
			break;

		case IDC_CHECK_BUZZER:
			if (SendDlgItemMessage(hDlg, IDC_CHECK_BUZZER, BM_GETCHECK, 0, 0)) 
				RunDialogStart(hDlg, ACT_SET_BUZZER, "set buzzer", true);
			else
				RunDialogStart(hDlg, ACT_SET_BUZZER, "set buzzer", false);
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

	g_hInst = hInst;

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
