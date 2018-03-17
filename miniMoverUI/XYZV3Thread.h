#ifndef XYZV3_THREAD_H
#define XYZV3_THREAD_H

#define XYZ_THREAD_DONE  (WM_APP + 100)

bool handleConvertFile(HWND hDlg, XYZV3 &xyz, int infoIdx);
bool handlePrintFile(HWND hDlg, XYZV3 &xyz);

#endif //XYZV3_THREAD_H
