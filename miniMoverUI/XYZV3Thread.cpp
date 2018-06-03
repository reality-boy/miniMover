
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <SDKDDKVer.h>
#include <Windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <commdlg.h>

#include "serial.h"
#include "network.h"
#include "timer.h"
#include "xyzv3.h"
#include "XYZV3Thread.h"

#pragma warning(disable:4996) // live on the edge!

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

// from miniMoverUI.cpp
extern int g_printPct;
void printFileCallback(float pct)
{
	g_printPct = (int)(pct * 100);
}

struct data
{
	HWND hWnd;
	XYZV3 *xyz;
	bool doPrint;
	char in[MAX_PATH];
	char out[MAX_PATH];
	int infoIdx;
};

DWORD __stdcall threadHandler(void *param)
{
	bool success = false;
	data *d = (data*)param;
	if(d && d->hWnd && d->xyz)
	{
		if(d->doPrint)
		{
			g_printPct = 0;
			success = d->xyz->printFile(d->in, printFileCallback);
			g_printPct = 0;
		}
		else
			success = d->xyz->convertFile(d->in, d->out, d->infoIdx);
	}
	
	//SendNotifyMessage
	SendMessage(d->hWnd, XYZ_THREAD_DONE, success, 0);

	return 0;
}

data d;
bool handleConvertFile(HWND hDlg, XYZV3 &xyz, int infoIdx)
{
	char *s;

	d.hWnd = hDlg;
	d.xyz = &xyz;
	d.doPrint = false;
	d.infoIdx = infoIdx;

	if(getFilePath(hDlg, d.in, sizeof(d.in), true))
	{
		// fix up path
		strcpy(d.out, d.in);
		s = strrchr(d.out, '.');
		if(s)
		{
			if(s[1] == 'g')
				sprintf(s, ".3w");
			else if(s[1] == '3')
				sprintf(s, ".gcode");

			if(getFilePath(hDlg, d.out, sizeof(d.out), false))
			{
				// Create the thread to begin execution on its own.
				CreateThread( NULL, 0, threadHandler, &d, 0, NULL);

				return true;
			}
		}
	}

	return false;
}

bool handlePrintFile(HWND hDlg, XYZV3 &xyz)
{
	d.out[0] = '\0';
	d.hWnd = hDlg;
	d.xyz = &xyz;
	d.doPrint = true;
	d.infoIdx = -1;

	if(getFilePath(hDlg, d.in, sizeof(d.in), true))
	{
		// Create the thread to begin execution on its own.
		CreateThread( NULL, 0, threadHandler, &d, 0, NULL);

		return true;
	}

	return false;
}
