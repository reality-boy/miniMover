
// includes

// tell windows to give us a more modern look
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

//#pragma warning(disable: 4995)
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
#include "xyzv3.h"
#include "XYZV3Thread.h"

// globals

XYZV3 xyz;

bool g_guiNeesUpdate = true;
bool g_threadRunning = false;

const int g_maxPorts = 24;
int g_comIDtoPort[g_maxPorts] = {-1};

// controls
HWND hwndTreeInfo = NULL;

HCURSOR waitCursor;
HCURSOR defaultCursor;

// source

#define ERR_C_BUFFER_SIZE 2048
void DebugPrint(char *format, ...)
{
	char msgBuf[ERR_C_BUFFER_SIZE];
	va_list arglist;

	va_start(arglist, format);
	_vsnprintf(msgBuf, sizeof(msgBuf), format, arglist);
	msgBuf[sizeof(msgBuf)-1] = '\0';
	va_end(arglist);

	OutputDebugString(msgBuf);
	OutputDebugString("\n");
}

//-------------------------------------------
// printing dialog


//-------------------------------------------
// main dialog

HTREEITEM treeAddNode(HWND hTree, HTREEITEM parrent, bool expand, bool leaf, const char *format, ...)
{
	TVINSERTSTRUCT tvnode;
	HTREEITEM hRet = NULL;

	char msgBuf[ERR_C_BUFFER_SIZE];
	va_list arglist;

	if(hTree)
	{
		va_start(arglist, format);
		_vsnprintf(msgBuf, sizeof(msgBuf), format, arglist);
		msgBuf[sizeof(msgBuf)-1] = '\0';
		va_end(arglist);

		(void)leaf; //****FixMe, set icon based on this
		tvnode.hParent = parrent;
		tvnode.hInsertAfter = TVI_LAST;
		tvnode.item.mask = TVIF_TEXT | TVIF_STATE;
		tvnode.item.pszText = msgBuf;
		tvnode.item.cchTextMax = 0;
		tvnode.item.state = (expand) ? TVIS_EXPANDED : 0;
		tvnode.item.stateMask = TVIS_EXPANDED;
		hRet = TreeView_InsertItem(hTree, &tvnode);
	}

	return hRet;
}


void MainDlgUpdateStatusTree(const XYZPrinterState *st, const XYZPrinterInfo *inf)
{
	HTREEITEM root, first;

	if(hwndTreeInfo && st && inf)
	{
		// don't repaint till we are done drawing
		LockWindowUpdate(hwndTreeInfo);

		// get scroll bar position
		first = TreeView_GetFirstVisible(hwndTreeInfo);

		//empty the tree
		TreeView_DeleteItem(hwndTreeInfo, NULL);

		root = treeAddNode(hwndTreeInfo, NULL, true, false, "Machine Info");

		// XYZPrinterInfo constants
		treeAddNode(hwndTreeInfo, root, true, false, "Name: %s", inf->screenName);
		//treeAddNode(hwndTreeInfo, root, true, false, "Given name: %s", st->machineName);
		//treeAddNode(hwndTreeInfo, root, true, false, "Model num: %s", st->info.modelNum);
		treeAddNode(hwndTreeInfo, root, true, false, "Model num: %s", st->machineModelNumber);
		treeAddNode(hwndTreeInfo, root, true, false, "File id: %s", inf->fileNum);
		//treeAddNode(hwndTreeInfo, root, true, false, "Serial num: %s", st->info.serialNum);
		treeAddNode(hwndTreeInfo, root, true, false, "Serial num: %s", st->machineSerialNum);

		treeAddNode(hwndTreeInfo, root, true, false, "Fillament serial: %s", st->filamentSerialNumber);
		treeAddNode(hwndTreeInfo, root, true, false, "Nozel serial: %s", st->nozelSerialNumber);
		treeAddNode(hwndTreeInfo, root, true, false, "Firmware ver: %s", st->firmwareVersion);

		treeAddNode(hwndTreeInfo, root, true, false, "File is v5: %d", inf->fileIsV5);
		treeAddNode(hwndTreeInfo, root, true, false, "File is zip: %d", inf->fileIsZip);
		//treeAddNode(hwndTreeInfo, root, true, false, "Com is v3: %d", inf->comIsV3);
		treeAddNode(hwndTreeInfo, root, true, false, "Build volume: %d l %d w %d h", inf->length, inf->width, inf->height);

		// XYZPrinterState
		treeAddNode(hwndTreeInfo, root, true, false, "Bed temp: %d C", st->bedTemp_C);
		treeAddNode(hwndTreeInfo, root, true, false, "Extruder temp: %d/%d C", st->extruderActualTemp_C, st->extruderTargetTemp_C);
		treeAddNode(hwndTreeInfo, root, true, false, "Fillament remain: %0.3f m", st->fillimantRemaining_mm / 1000.0f);

		treeAddNode(hwndTreeInfo, root, true, false, "Is PLA: %d", st->isFillamentPLA);
		treeAddNode(hwndTreeInfo, root, true, false, "Nozel ID: %d", st->nozelID);
		treeAddNode(hwndTreeInfo, root, true, false, "Nozel Diam: %0.2f mm", st->nozelDiameter_mm);
		treeAddNode(hwndTreeInfo, root, true, false, "Calib: %d,%d,%d,%d,%d,%d,%d,%d,%d", 
														st->calib[0], st->calib[1], st->calib[2],
														st->calib[3], st->calib[4], st->calib[5],
														st->calib[6], st->calib[7], st->calib[8]);
		treeAddNode(hwndTreeInfo, root, true, false, "Auto level: %d", st->autoLevelEnabled);
		treeAddNode(hwndTreeInfo, root, true, false, "Buzzer: %d", st->buzzerEnabled);

		treeAddNode(hwndTreeInfo, root, true, false, "Print complete: %d %", st->printPercentComplete);
		treeAddNode(hwndTreeInfo, root, true, false, "Print elapsed: %d min", st->printElapsedTime_m);
		treeAddNode(hwndTreeInfo, root, true, false, "Print remain: %d min", st->printTimeLeft_m);

		treeAddNode(hwndTreeInfo, root, true, false, "Error: %d", st->errorStatus);
		treeAddNode(hwndTreeInfo, root, true, false, "Status: (%d:%d) %s", st->printerStatus, st->printerSubStatus, st->printerStatusStr);
		treeAddNode(hwndTreeInfo, root, true, false, "Packet size: %d", st->packetSize);
		//****FixMe, convert these to day/hour/min
		treeAddNode(hwndTreeInfo, root, true, false, "Lifetime on: %d min", st->printerLifetimePowerOnTime_min);
		treeAddNode(hwndTreeInfo, root, true, false, "Last power on: %d min", st->printerLastPowerOnTime_min);
		treeAddNode(hwndTreeInfo, root, true, false, "Power on: %d min", st->extruderLifetimePowerOnTime_min);

		// scroll tree
		TreeView_SelectSetFirstVisible(hwndTreeInfo, first);

		// now repaint all at once
		LockWindowUpdate(NULL);
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

void MainDlgConnect(HWND hDlg)
{
	int comID = SendDlgItemMessage(hDlg, IDC_COMBO_PORT, CB_GETCURSEL, 0, 0);
	if(comID == CB_ERR)
		comID = 0;

	if(xyz.connect(g_comIDtoPort[comID]))
		MainDlgSetStatus(hDlg, "connected");
	else
		MainDlgSetStatus(hDlg, "not connected");

	g_guiNeesUpdate = true;
}

void MainDlgUpdate(HWND hDlg)
{
	(void)hDlg;

	if(g_guiNeesUpdate)
	{
		g_guiNeesUpdate = false;

		SetCursor(waitCursor);
		if(xyz.updateStatus())
		{
			const XYZPrinterState *st = xyz.getPrinterState();
			const XYZPrinterInfo *inf = xyz.getPrinterInfo();

			if(st->isValid)
			{
				MainDlgUpdateStatusTree(st, inf);

				SendDlgItemMessage(hDlg, IDC_CHECK_BUZZER, BM_SETCHECK, (WPARAM)(st->buzzerEnabled) ? BST_CHECKED : BST_UNCHECKED, 0);
				SendDlgItemMessage(hDlg, IDC_CHECK_AUTO, BM_SETCHECK, (WPARAM)(st->autoLevelEnabled) ? BST_CHECKED : BST_UNCHECKED, 0);

				SetDlgItemInt(hDlg, IDC_EDIT_ZOFF, st->zOffset, false);
			}
		}
		SetCursor(defaultCursor);
	}
}

BOOL CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg) 
    {
        case WM_INITDIALOG:
			defaultCursor = LoadCursor(NULL, IDC_ARROW);    // default cursor
			waitCursor = LoadCursor(NULL, IDC_WAIT);     // wait cursor

			MainDlgUpdateComDropdown(hDlg);
			MainDlgConnect(hDlg);

			hwndTreeInfo = GetDlgItem(hDlg, IDC_TREE_STATUS);
			if(hwndTreeInfo) // may help reduce flicker
				TreeView_SetExtendedStyle(hwndTreeInfo, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

			SendDlgItemMessage(hDlg, IDC_SPIN_ZOFF, UDM_SETRANGE, 0, MAKELONG( 1000, 1));
            break;

        case WM_DESTROY:
            // Cleanup everything
			PostQuitMessage(0);
            break;

		case WM_CLOSE:
			DestroyWindow(hDlg);
			xyz.disconnect();
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

        case WM_COMMAND:

			//MessageBeep(MB_ICONEXCLAMATION);
			// do nothing if thread alrady running
			if(g_threadRunning)
				break;

            switch(LOWORD(wParam))
            {
			case IDOK:
				if(GetFocus() == GetDlgItem(hDlg, IDC_EDIT_ZOFF))
				{
					SetCursor(waitCursor);
					MainDlgSetStatus(hDlg, "set z-offset");
					if(xyz.setZOffset(GetDlgItemInt(hDlg, IDC_EDIT_ZOFF, NULL, false)))
					{
						MainDlgSetStatus(hDlg, "set z-offset complete");
						g_guiNeesUpdate = true;
					}
					else
						MainDlgSetStatus(hDlg, "set z-offset failed");
					SetCursor(defaultCursor);
				}
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

			case IDC_BUTTON_REFRESH:
				g_guiNeesUpdate = true;
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

			case IDC_BUTTON_CONVERT:
				MainDlgSetStatus(hDlg, "converting file");
				SetCursor(waitCursor);

				g_threadRunning = handleConvertFile(hDlg, xyz);
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
					MessageBox(NULL, "Hit ok when filliment comes out of nozel.", "Load Fillament", MB_OK);
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
				MainDlgSetStatus(hDlg, "cleaning nozel");
				if(xyz.cleanNozzleStart())
				{
					MessageBox(NULL, "Hit ok after cleaning nozel.", "Clean Nozel", MB_OK);
					if(xyz.cleanNozzleFinish())
						MainDlgSetStatus(hDlg, "cleaning nozel complete");
					else
						MainDlgSetStatus(hDlg, "cleaning nozel failed");
				}
				else
					MainDlgSetStatus(hDlg, "cleaning nozel failed");
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
				{
					SetCursor(waitCursor);
					MainDlgSetStatus(hDlg, "set z-offset");
					if(xyz.setZOffset(GetDlgItemInt(hDlg, IDC_EDIT_ZOFF, NULL, false)))
					{
						MainDlgSetStatus(hDlg, "set z-offset complete");
						g_guiNeesUpdate = true;
					}
					else
						MainDlgSetStatus(hDlg, "set z-offset failed");
					SetCursor(defaultCursor);
				}
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
				g_guiNeesUpdate = true;
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
				g_guiNeesUpdate = true;
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

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, INT)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	INITCOMMONCONTROLSEX ex;
	ex.dwSize = sizeof(ex);
	ex.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&ex);

	timeBeginPeriod(1); // get 1 millisecond timers

    HWND main_hWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, MainDlgProc);

    int status = 1;
    if(main_hWnd)
    {
	    MSG  msg;
	    while(status > 0)
		{
			//don't let windows stop us from processing messages
			int maxCount = 50;

			//peak so we don't stall waiting for a message from windows
			//but pump all windows messages
			while(--maxCount && 
				status > 0 &&  
				PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) ) 
			{  
				//crank message pump to make windows happy
				status = GetMessage (& msg, 0, 0, 0);
				if(status > 0)
			    {
			        if (!IsDialogMessage (main_hWnd, & msg))
			        {
			            TranslateMessage ( & msg );
			            DispatchMessage ( & msg );
			        }
			    }
			}  

			// do the real work
			MainDlgUpdate(main_hWnd);

			Sleep(100);
		}
		//do shut down code here
		DestroyWindow(main_hWnd);
    }

	timeEndPeriod(1); // release 1 millisecond timer

	return status;
}
