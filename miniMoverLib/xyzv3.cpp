#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
# include <SDKDDKVer.h>
# include <Windows.h>
# pragma warning(disable:4996) // live on the edge!
#else
#include <unistd.h>
//****FixMe, deal with these more properly!
# define GetTempPath(MAX_PATH, tpath)(tpath[0] = '\0')
# define GetTempFileName(tpath, a, b, tfile)(strcpy(tfile, "temp.tmp"))
#endif

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

#include "debug.h"

// prune out unuzed miniz features
#define MINIZ_NO_STDIO					// no file I/O support
#define MINIZ_NO_TIME					// disable file time routines
#define MINIZ_NO_ZLIB_APIS				// remove zlib style calls
#define MINIZ_NO_MALLOC					// strip out malloc
#include "miniz.h"

#include "timer.h"
#include "aes.h"
#include "stream.h"
#include "serial.h"
#include "socket.h"
#include "xyzv3.h"

// uncomment to test out experimental v2 protocol
//#define USE_V2
// uncomment to force wifi connections to close between messages
//#define CLOSE_ON_WIFI

const char *g_ver = "v0.9.6 Beta";

const XYZPrinterInfo XYZV3::m_infoArray[m_infoArrayLen] = { //  File parameters        Machine capabilities
	//   modelNum,       fileNum, serialNum,          webNum,    IsV5, IsZip, comV3,   tmenu, hbed,  dExtr, wifi,  scan, laser,    len, wid, hgt,   screenName
	{"dvF100B000",   "daVinciF10", "3DP01P", "davincif10_V2",   false,  true, false,    true,  true, false, false, false, false,   200, 200, 200,   "da Vinci 1.0"},			// F10
	{"dvF100A000",   "daVinciF10", "3F10AP", "dvF100A000_V2",   false,  true, false,    true,  true, false, false, false, false,   200, 200, 200,   "da Vinci 1.0A"},			// F10A
	{"dvF10SA000",   "daVinciF10", "3S10AP", "dvF10SA000_V2",   false,  true, false,    true,  true, false, false,  true,  true,   200, 200, 190,   "da Vinci 1.0 AiO"},		// F10SA, possibly v3?
	{"dvF110B000",   "daVinciF11", "3F11XP", "dvF110B000_V2",   false,  true,  true,    true,  true, false,  true, false, false,   200, 200, 200,   "da Vinci 1.1 Plus"},		// F11, supports both v2 and v3 protocol, but only query status and send file
//	{"dvF110A000",..., "da Vinci 1.1A"},					// F11A
//	{"dvF110P000",..., "da Vinci 1.1 Plus Plus"},			// F11P ???
//	{"dvF11SA000",..., "da Vinci 1.1 AiO"},					// F11SA
	{"dvF200B000",   "daVinciF20", "3F20XP", "davincif20_V2",   false,  true, false,    true,  true,  true, false, false, false,   200, 200, 150,   "da Vinci 2.0 Duo"},		// F20
	{"dvF200A000",   "daVinciF20", "3F20AP", "dvF200A000_V2",   false,  true, false,    true,  true,  true, false, false, false,   200, 200, 150,   "da Vinci 2.0A Duo"},		// F20A, possibly v3
//	{"dvF210B000",   "daVinciF21",..., "da Vinci 2.1 Duo"}, // F21
//	{"dvF210A000",..., "da Vinci 2.1A Duo"},				// F21A
//	{"dvF210P000",..., "da Vinci 2.1 Duo Plus"},			// F21P
//	{"??????????",..., "da Vinci 2.1 Duo Plus (AIO)"},		// F21S
	{"dv1J00A000",  "daVinciJR10", "3F1J0X", "dv1J00A000_V2",   false, false,  true,    true, false, false, false, false, false,   150, 150, 150,   "da Vinci Jr. 1.0"},		// FJR10, all jr's support optional 0.3mm nozzle
	{"dv1JA0A000",   "dv1JA0A000", "3F1JAP", "dv1JA0A000_V2",   false, false,  true,    true, false, false, false, false,  true,   175, 175, 175,   "da Vinci Jr. 1.0A"},		// FJR10A
	{"dv1JW0A000", "daVinciJR10W", "3F1JWP", "dv1JW0A000_V2",   false, false,  true,    true, false, false,  true, false, false,   150, 150, 150,   "da Vinci Jr. 1.0w"},		// FJR10W
	{"dv1JS0A000",   "dv1JSOA000", "3F1JSP", "dv1JS0A000_V2",   false, false,  true,    true, false, false,  true,  true,  true,   150, 150, 150,   "da Vinci Jr. 1.0 3in1"},	// F1JS, daVinciJR10S 
	{"dv1JSOA000", "daVinciJR10S", "3F1JOP", "?????????????",    true, false,  true,    true, false, false,  true,  true,  true,   150, 150, 150,   "da Vinci Jr. 1.0 3in1 (Open filament)"}, // F1JO, not sure this is right, file num is suspicioud
	{"dv2JW0A000", "daVinciJR20W", "3F2JWP", "dv2JW0A000_V2",   false, false,  true,    true, false,  true,  true, false, false,   150, 150, 150,   "da Vinci Jr. 2.0 Mix"},	// FJR20M, only 0.4mm nozzle

	{"dv1MX0A000",   "dv1MX0A000", "3FM1XP", "dv1MX0A000_V2",   false, false,  true,   false, false, false, false, false, false,   150, 150, 150,   "da Vinci miniMaker"},		// FM1X
	{"dv1MW0A000",   "dv1MW0A000", "3FM1WP", "dv1MW0A000_V2",   false, false,  true,   false, false, false,  true, false, false,   150, 150, 150,   "da Vinci mini w"},			// FM1W
	{"dv1MW0B000",   "dv1MW0B000", "3FM1JP", "dv1MW0B000_V2",   false, false,  true,   false, false, false,  true, false, false,   150, 150, 150,   "da Vinci mini wA"},
	{"dv1MW0C000",   "dv1MW0C000", "3FM3WP", "dv1MW0C000_V2",   false, false,  true,   false, false, false,  true, false, false,   150, 150, 150,   "da Vinci mini w+"},
	{"dv1NX0A000",   "dv1NX0A000", "3FN1XP", "dv1NX0A000_V2",   false, false,  true,   false, false, false, false, false, false,   120, 120, 120,   "da Vinci nano"},			// FN1X, 3FNAXP, there are two nanos, an orange with 0.3 mm nozzl3 and white with 0.4 mm nozzle
	{"dv1NW0A000",   "dv1NW0A000", "3FNAWP", "dv1NW0A000_V2",   false, false,  true,   false, false, false,  true, false, false,   120, 120, 120,   "da Vinci nano w"},

	{"dv1JP0A000",   "dv1JP0A000", "3F1JPP", "dv1JP0A000_V2",    true, false,  true,    true, false, false, false, false, false,   150, 150, 150,   "da Vinci Jr. 1.0 Pro"},	// FJR10PRO
	{"dv1JPWA000",   "dv1JPWA000", "3F1JPW", "dv1JPWA000_V2",    true, false,  true,    true, false, false,  true, false, false,   150, 150, 150,   "da Vinci Jr. 1.0w Pro"},
//  "da Vinci Jr. 1.0 Pro X" // F1JPX
	{"dvF1W0A000",  "daVinciAW10", "3F1AWP", "dvF1W0A000_V2",    true, false,  true,    true,  true, false,  true, false,  true,   200, 200, 200,   "da Vinci 1.0 Pro"},		// F1AW
//	{"dv1JA0A000",   "dv1JA0A000", "3F1JAA", "dv1JA0A000_V2",    true, false,  true,    true,  true, false,  true, false,  true,   200, 200, 200,   "da Vinci 1.0A Pro"},
	{"dvF1WSA000",  "daVinciAS10", "3F1ASP", "dvF1WSA000_V2",    true, false,  true,    true,  true, false,  true,  true,  true,   200, 200, 190,   "da Vinci 1.0 Pro 3in1"},	// F1AS
	{"dv1SW0A000",   "dv1SW0A000", "3F1SWP", "dv1SW0A000_V2",    true, false,  true,    true,  true, false,  true, false,  true,   300, 300, 300,   "da Vinci Super"},			// F1SW, 0.4mm/0.6mm/0.8mm nozzles
//	{"dv1CP0A000",             "", "3FC1XP", "dv1CP0A000_V2",    true,  false, true,    true, false, false,  true, false, false,   200, 200, 150,   "da Vinci Color"}, // no heated bed, but full color! // does not support 3w file format
//  {"dv1CPSA000",             "", "3FC1SP", "dv1CPSA000_V2",..., 200,200,150,"XYZprinting da Vinci Color AiO"}, //PartPro200 xTCS
//  {"dv1MWCA000", "da Vinci Color mini"}, //F1CORM
//  da Vinci Color 5D
};

	//  unknown

	//       da Vinci PartPro200 xTCS // F1CORA
	//3F3PMP XYZprinting da Vinci PartPro 300xT //F10PXT
	//3FWB1P XYZprinting WiFi Box 
	//3F21X  da Vinci 2.1 Duo Plus
	//3FCM1  da Vinci ?
	//3FJPW  da Vinci ?
	//3F10I  da Vinci ?           
	//3L10A  Nobel SuperFine DLP         
	//3L10X  Nobel ?
	//****Note, a fileNum of FXX may be a universal demo file marker that any printer can load
	// 3D Desktop Scanner // D10
	// 3D Scanner 1.0 // H10
	// 3D Scanner 1.0A // H10A

	/*
	//dvF100B000 daVinciF10 davincif10_V2
	//dvF200B000 daVinciF20 davincif20_V2
	//daVinciF11 davincif11_V2
	//daVinciF21
	//daVinciJR10
	//daVinciJR10W
	//daVinciAW10
	//daVinciJR10S
	//daVinciAS10
	//daVinciJR20W

	daVinciF10a
	daVinciF20a
	daVinciF10sa
	daVinciJR11
	daVinciJR20
	
	//dvF200A000 dvF200A000_V2
	//dvF100A000 dvF100A000_V2
	//dvF10SA000 dvF10SA000_V2
	//dvF110B000 dvF110B000_V2
	//dvF110A000 
	//dvF110P000
	//dvF11SA000
	//dvF210B000
	//dvF210A000
	//dvF210P000
	//dv1J00A000 dv1J00A000_V2
	//dv1JW0A000 dv1JW0A000_V2 JR10
	//dv1JS0A000 dv1JS0A000_V2 JR10
	//dv2JW0A000 dv2JW0A000_V2
	//dv1JA0A000 dv1JA0A000_V2 JR10A
	//dvF1W0A000 dvF1W0A000_V2 AW10?
	//dvF1WSA000 dvF1WSA000_V2 AS10?
	//dv1MW0A000 dv1MW0A000_V2 mini w
	//dv1MX0A000 dv1MX0A000_V2 miniMaker
	//dv1NX0A000 dv1NX0A000_V2 nano
	//dv1NW0A000 dv1NW0A000_V2 nano
	//dv1MW0C000 dv1MW0C000_V2 mini w
	//dv1SW0A000 dv1SW0A000_V2 da Vinci Super
	//dv1JP0A000 dv1JP0A000_V2 da Vinci Jr Pro
	//dv1JSOA000 dv1JS0A000_V2
	//dv1CP0A000 dv1CP0A000_V2 da Vinci Color
	//dv1JPWA000 dv1JPWA000_V2 da Vinci Jr 1.0w Pro
	//           dv1MW0B000_V2
	
	dv1PW3A000 dv1PW3A000_V2 da Vinci Part Pro
	dv1MWCA000 dv1MWCA000_V2 da Vinci Color Mini
	dv1JZ0A000 dv1JZ0A000_V2
	dv1JPXA000 dv1JPXA000_V2
	           dv1WB0A000_V2
	*/

// internal action state
enum ActState
{
	ACT_FAILURE,			// something went wrong
	ACT_SUCCESS,			// something went right

	// Calibrate Bed
	ACT_CB_START,			// start
	ACT_CB_START_SUCCESS,	// wait on success
	ACT_CB_HOME,			// wait for signal to lower detector
	ACT_CB_ASK_LOWER,		// ask user to lower detector
	ACT_CB_LOWERED,			// notify printer detecotr was lowered
	ACT_CB_CALIB_START,		// wait for calibration to start
	ACT_CB_CALIB_RUN,		// wait for calibration to finish
	ACT_CB_ASK_RAISE,		// ask user to raise detector
	ACT_CB_RAISED,			// notify printer detector was raised
	ACT_CB_COMPLETE,		// wait for end of calibration

	// Clean Nozzle
	ACT_CL_START,
	ACT_CL_START_SUCCESS,
	ACT_CL_WARMUP_COMPLETE,
	ACT_CL_CLEAN_NOZLE,
	ACT_CL_FINISH,
	ACT_CL_COMPLETE,

	// Home printer
	ACT_HP_START,
	ACT_HP_START_SUCCESS,
	ACT_HP_HOME_COMPLETE,

	// Jog Printer
	ACT_JP_START,
	ACT_JP_START_SUCCESS,
	ACT_JP_JOG_COMPLETE,

	// Load Fillament
	ACT_LF_START,
	ACT_LF_START_SUCCESS,
	ACT_LF_HEATING,
	ACT_LF_LOADING,
	ACT_LF_WAIT_LOAD,
	ACT_LF_LOAD_FINISHED,
	ACT_LF_LOAD_COMPLETE,

	// Unload Fillament
	ACT_UF_START,
	ACT_UF_START_SUCCESS,
	ACT_UF_HEATING,
	ACT_UF_UNLOADING,
	ACT_UF_CANCEL, // only get here if cancel button pressed
	ACT_UF_CANCEL_COMPLETE, // only get here if cancel button pressed
	ACT_UF_UNLOAD_COMPLETE,

	// convert file
	ACT_CF_START,
	ACT_CF_COMPLETE,

	// print file
	ACT_PF_START,
	ACT_PF_SEND,
	ACT_PF_SEND_PROCESS,
	ACT_PF_COMPLETE,

	// upload firmware
	ACT_FW_START,
	ACT_FW_SEND_PROCESS,
	ACT_FW_COMPLETE,

	// query status
	ACT_QS_START,
	ACT_QS_CHECK_FOR_LINE,

	// simple command
	ACT_SC_START,
	ACT_SC_GET_ZOFFSET,
	ACT_SC_GET_ENDCOM,
	ACT_SC_COMPLETE,
};

XYZV3::XYZV3() 
{
	debugPrint(DBG_LOG, "XYZV3::XYZV3()");

	init();
} 

XYZV3::~XYZV3() 
{
	debugPrint(DBG_LOG, "XYZV3::~XYZV3()");
} 

void XYZV3::init()
{
	debugPrint(DBG_LOG, "XYZV3::init()");

	memset(&m_status, 0, sizeof(m_status));
	memset(&pDat, 0, sizeof(pDat));
	m_stream = NULL;
	m_info = NULL;
	m_actState = ACT_FAILURE;
	m_actSubState = ACT_FAILURE;
	m_needsMachineState = false;
	m_progress = 0;

	m_jogAxis = ' ';
	m_jogDist_mm = 0;
	m_infoIdx = -1;
	m_fileIsTemp = false;
	m_filePath[0] = '\0';
	m_fileOutPath[0] = '\0';
#ifdef USE_V2
	m_useV2Protocol = true;
#else
	m_useV2Protocol = false;
#endif
	m_V2Token[0] = '\0';
}

void XYZV3::startMessage()
{
	// on wifi, we need to reopen the stream before each command
	if(m_stream)
	{
		if( 
#ifdef CLOSE_ON_WIFI
			m_stream->isWIFI() || 
#endif
			!m_stream->isOpen())
		{
			m_stream->reopenStream();
		}

		m_stream->clear();
	}
}

void XYZV3::endMessage()
{
	if(m_stream)
	{
		//****FixMe, some printers return '$\n\n' after 'ok\n'
		// drain any left over messages
		m_stream->clear();

#ifdef CLOSE_ON_WIFI
		if(m_stream->isWIFI())
			m_stream->closeStream();
#endif
	}
}

void XYZV3::setStream(StreamT *s)
{
	debugPrint(DBG_LOG, "XYZV3::setStream(%d)", s);

	// close out old stream
	if(m_stream)
		m_stream->closeStream();

	//reinitialize everything
	init();

	// and put new in its place
	m_stream = s;
	if(m_stream)
		m_stream->clear();
}

const StreamT* XYZV3::getStream()
{
	debugPrint(DBG_LOG, "XYZV3::getStream()");
	return m_stream;
}

bool XYZV3::serialCanSendMessage()
{
	//if(isWIFI())
	//	return m_sDelay.isTimeout();
	//else
		return true;
}

bool XYZV3::serialSendMessage(const char *format, ...)
{
	debugPrint(DBG_LOG, "XYZV3::serialSendMessage(%s)", format);

	bool success = false;

	if(m_stream && m_stream->isOpen() && format)
	{
		const int BUF_SIZE = 2048;
		char msgBuf[BUF_SIZE];
		va_list arglist;

		va_start(arglist, format);
		vsnprintf(msgBuf, sizeof(msgBuf), format, arglist);
		msgBuf[sizeof(msgBuf)-1] = '\0';
		va_end(arglist);

		m_stream->writeStr(msgBuf);
		m_sDelay.setTimeout_s(3);

		success = true;
	}
	else
		debugPrint(DBG_WARN, "XYZV3::serialSendMessage invalid input");

	return success;
}

XYZPrintStateCode XYZV3::translateStatus(int oldStatus)
{
	// translate old printer status codes
	switch(oldStatus)
	{
	case 0: return PRINT_INITIAL;
	case 1: return PRINT_HEATING;
	case 2: return PRINT_PRINTING;
	case 3: return PRINT_CALIBRATING;
	case 4: return PRINT_CALIBRATING_DONE;
	case 5: return PRINT_COOLING_DONE;
	case 6: return PRINT_COOLING_END;
	case 7: return PRINT_ENDING_PROCESS;
	case 8: return PRINT_ENDING_PROCESS_DONE;
	case 9: return PRINT_JOB_DONE;
	case 10: return PRINT_NONE;
	case 11: return PRINT_IN_PROGRESS;
	case 12: return PRINT_STOP;
	case 13: return PRINT_LOAD_FILAMENT;
	case 14: return PRINT_UNLOAD_FILAMENT;
	case 15: return PRINT_AUTO_CALIBRATION;
	case 16: return PRINT_JOG_MODE;
	case 17: return PRINT_FATAL_ERROR;
	default: return (XYZPrintStateCode)oldStatus;
	}
}

int XYZV3::translateErrorCode(int code)
{
	// 0x40000000 == extruder 1
	// 0x20000000 == extruder 2
	switch(code & 0xFFFF)
	{
	case 0x0002: return 0x0101;
	case 0x0003: return 0x001B; // machine buisy
//	case 0x0003: return 0x0102; // bead heat error
	case 0x0004: return 0x0103;
	case 0x0005: return 0x0104;
	case 0x0006: return 0x0105;
	case 0x0009: return 0x0108;
	case 0x000A: return 0x0109;
	case 0x000B: return 0x010A;
	case 0x0012: return 0x010B;
	case 0x000E: return 0x0202;
	case 0x000F: return 0x0203;
	case 0x0011: return 0x0205;
	case 0x001C: return 0x0206;
	case 0x001D: return 0x0207;
	case 0x0007: return 0x0208;
	case 0x0008: return 0x0209;
	case 0x020B: return 0x020C;
	case 0x0014: return 0x0401;
	case 0x0015: return 0x0402;
	case 0x0017: return 0x0404;
	case 0x0010: return 0x0405;
	case 0x0018: return 0x0405;

	default: return code & 0xFFFF;
	}
}

void XYZV3::parseStatusSubstring(const char *str)
{
	debugPrint(DBG_VERBOSE, "XYZV3::parseStatusSubstring(%s)", str);

	if(str && str[0] != '\0')
	{
		if(str[0] == '$' || str[0] == 'E' || str[0] == 'o' || str[1] == ':')
		{
			const char *strPtr = NULL;
			char s1[256] = "";
			int t;

			switch(str[0])
			{
			case 'b': // heat bed temperature, b:xx - deg C
				sscanf(str, "b:%d", &m_status.bBedActualTemp_C);
				break;

			case 'c': // Calibration values, c:{x1,x2,x3,x4,x5,x6,x7,x8,x9} 
				//only on miniMover
				m_status.cCalibIsValid = true;
				sscanf(str, "c:{%d,%d,%d,%d,%d,%d,%d,%d,%d}",
					&m_status.cCalib[0], 
					&m_status.cCalib[1], 
					&m_status.cCalib[2], 
					&m_status.cCalib[3], 
					&m_status.cCalib[4], 
					&m_status.cCalib[5], 
					&m_status.cCalib[6], 
					&m_status.cCalib[7], 
					&m_status.cCalib[8]);
				break;

			case 'd': // print status, d:ps,el,es
				//   ps - print percent complete (0-100?)
				//   el - print elapsed time (minutes)
				//   es - print estimated time left (minutes)
				//   a value of 0,0,0 indicates no job is running
				sscanf(str, "d:%d,%d,%d", &m_status.dPrintPercentComplete, &m_status.dPrintElapsedTime_m, &m_status.dPrintTimeLeft_m);

				// why is this set to this? is it a bad 3w file?
				if(m_status.dPrintTimeLeft_m == 0x04444444)
					m_status.dPrintTimeLeft_m = -1;
				break;

			case 'e': // error status, e:ec - some sort of string?
				t = 0;
				sscanf(str, "e:%d", &t);
				// 0x40 is extruder 1
				// 0x20 is extruder 2
				// 0x00 is everything else
				m_status.eErrorStatusHiByte = (t & 0xFF000000) >> 24;
				m_status.eErrorStatus = translateErrorCode(t);

				strPtr = errorCodeToStr(m_status.eErrorStatus);
				if(strPtr)
					strcpy(m_status.eErrorStatusStr, strPtr);
				else
					m_status.eErrorStatusStr[0] = '\0';
				break;

			case 'f': // filament remaining, f:ct,len,len2
				//   ct - how many spools of filiment, 1 for normal printer
				//   len - filament 1 left in millimeters
				//   len2 - filament 2 left in millimeters, optional
				sscanf(str, "f:%d,%d,%d", &m_status.fFilamentSpoolCount, &m_status.fFilament1Remaining_mm, &m_status.fFilament2Remaining_mm);
				break;

			//case 'g': break; // unused

			case 'h': // pla filament loaded, h:x > 0 if pla filament in printer
				// not used with miniMaker
				sscanf(str, "h:%d", &m_status.hIsFilamentPLA);
				break;

			case 'i': // machine serial number, i:sn - serial number
				//****Note, convert ? characters to - characters when parsing sn
				sscanf(str, "i:%s", m_status.iMachineSerialNum);
				break;

			case 'j': // printer status, j:st,sb
				//   st - status id
				//   sb - substatus id
				t = 0;
				sscanf(str, "j:%d,%d", &t, &m_status.jPrinterSubState);
				m_status.jPrinterState = translateStatus(t);

				// fill in status string
				strPtr = stateCodesToStr(m_status.jPrinterState, m_status.jPrinterSubState);
				if(strPtr)
					strcpy(m_status.jPrinterStateStr, strPtr); 
				else 
					m_status.jPrinterStateStr[0] = '\0';
				break;

			case 'k': // material type, k:xx
				//   xx is material type?
				//   one of 41 46 47 50 51 54 56
				//not used on miniMaker
				sscanf(str, "k:%d", &m_status.kFilamentMaterialType);

				strPtr = filamentMaterialTypeToStr(m_status.kFilamentMaterialType);
				if(strPtr)
					strcpy(m_status.kFilamentMaterialTypeStr, strPtr);
				else 
					m_status.kFilamentMaterialTypeStr[0] = '\0';

				break;

			case 'l': // language, l:ln - one of en, fr, it, de, es, jp, tw, cn, kr
				sscanf(str, "l:%s", m_status.lLang);
				break;

			case 'm': // ????? m:x,y,z
				//****FixMe, work out what this is
				sscanf(str, "m:%d,%d,%d", &m_status.mVal[0], &m_status.mVal[1], &m_status.mVal[2]);
				break;

			case 'n': // printer name, n:nm - name as a string
				//sscanf(str, "n:%s", m_status.nMachineName); // stops scanning at first space!
				strcpy(m_status.nMachineName, str+2);
				break;

			case 'o': // print options, o:ps,tt,cc,al
				if(str[1] == 'k')
				{
				}
				else
				{
					//   ps is package size * 1024
					//   tt ??? //****FixMe, work out what this is
					//   cc ??? //****FixMe, work out what this is
					//   al is auto leveling on if a+
					//o:p8,t1,c1,a+
					sscanf(str, "o:p%d,t%d,c%d,%s", 
						&m_status.oPacketSize, 
						&m_status.oT, 
						&m_status.oC, 
						s1);
					m_status.oPacketSize = (m_status.oPacketSize > 0) ? m_status.oPacketSize*1024 : 8192;
					m_status.oAutoLevelEnabled = (0 == strcmp(s1, "a+")) ? true : false;
				}
				break;

			case 'p': // printer model number, p:mn - model_num
				//p:dv1MX0A000
				sscanf(str, "p:%s", m_status.pMachineModelNumber);
				m_info = XYZV3::modelToInfo(m_status.pMachineModelNumber);
				break;

			//case 'q': break; // unused
			//case 'r': break; // unused

			case 's': // machine capabilities, s:{xx,yy...}
				//   xx is one of
				//   button:no
				//   buzzer:on  can use buzzer?
				//   dr:{front:on,top:on}  front/top door 
				//   eh:1  lazer engraver installed
				//   fd:1  ???
				//   fm:1  ???
				//   of:1  open filament allowed
				//   sd:yes  sd card yes or no
				//s:{"fm":0,"fd":1,"sd":"yes","button":"no","buzzer":"on"}
				//s:{"fm":1,"fd":1,"dr":{"top":"off","front":"off"},"sd":"yes","eh":"0","of":"1"}
				//****FixMe, need to detect if status is available or not, and indicate if feature is present
				if(findJsonVal(str, "buzzer", s1, sizeof(s1)))
					m_status.sBuzzerEnabled = (0==strcmp(s1, "on")) ? true : false;
				if(findJsonVal(str, "button", s1, sizeof(s1)))
					m_status.sButton = (0==strcmp(s1, "yes")) ? true : false;

				char tstr[512];
				if(findJsonVal(str, "dr", tstr, sizeof(tstr)))
				{
					if(findJsonVal(tstr, "top", s1, sizeof(s1)))
						m_status.sFrontDoor = (0==strcmp(s1, "on")) ? true : false;
					if(findJsonVal(tstr, "front", s1, sizeof(s1)))
						m_status.sTopDoor = (0==strcmp(s1, "on")) ? true : false;
				}

				if(findJsonVal(str, "sd", s1, sizeof(s1)))
					m_status.sSDCard = (0==strcmp(s1, "yes")) ? true : false;
				if(findJsonVal(str, "eh", s1, sizeof(s1)))
					m_status.sHasLazer = (s1[0] == '1') ? true : false;
				if(findJsonVal(str, "fd", s1, sizeof(s1)))
					m_status.sFd = (s1[0] == '1') ? true : false;
				if(findJsonVal(str, "fm", s1, sizeof(s1)))
					m_status.sFm = (s1[0] == '1') ? true : false;
				if(findJsonVal(str, "of", s1, sizeof(s1)))
					m_status.sOpenFilament = (s1[0] == '1') ? true : false;
				break;

			case 't': // extruder temperature, t:ss,aa,bb,cc,dd
				{
					//   if ss == 1
					//     aa is extruder temp in C
					//     bb is target temp in C
					//   else
					//     aa is extruder 1 temp
					//     bb is extruder 2 temp
					//t:1,20,0
					int t = 0;
					sscanf(str, "t:%d,%d,%d", &m_status.tExtruderCount, &m_status.tExtruder1ActualTemp_C, &t);
					if(m_status.tExtruderCount == 1)
					{
						m_status.tExtruderTargetTemp_C = t; // set by O: if not set here
						m_status.tExtruder2ActualTemp_C = 0;
					}
					else
					{
						//m_status.tExtruderTargetTemp_C = 0; // set by O: if not set here
						m_status.tExtruder2ActualTemp_C = t;
					}
				}
				break;

			//case 'u': break; // unused

			case 'v': // firmware version, v:fw or v:os,ap,fw
				//   fw is firmware version string
				//   os is os version string
				//   ap is app version string
				//v:1.1.1
				if(strchr(str, ','))
					sscanf(str, "v:%[^,],%[^,],%s", m_status.vOsFirmwareVersion, m_status.vAppFirmwareVersion, m_status.vFirmwareVersion);
				else
					sscanf(str, "v:%s", m_status.vFirmwareVersion);
				break;

			case 'w': // filament serian number, w:id,sn,xx
				//   if id == 1
				//     sn is filament 1 serial number
				//     xx is optional default filament temp
				//   else
				//     sn is filament 1 serial number
				//     xx is filament 2 serial number
				//
				//   Serial number format
				//   DDMLCMMTTTSSSS
				//   	DD - Dloc
				//   	M - Material
				//   	L - Length
				//   	    varies but in general 
				//   	    3 - 120000 mm
				//   	    5 - 185000 mm
				//   	    6 - 240000 mm
				//   	C - color
				//   	MM - Mloc
				//   	TTT - Mdate
				//   	SSSS - serial number
				//w:1,PMP6PTH6840596
				sscanf(str, "w:%d,%[^,],%s", &m_status.wFilamentCount, m_status.wFilament1SerialNumber, m_status.wFilament2SerialNumber);

				if(*m_status.wFilament1SerialNumber != '-' && strlen(m_status.wFilament1SerialNumber) > 4)
				{
					strPtr = filamentColorIdToStr(m_status.wFilament1SerialNumber[4]);
					if(strPtr)
						strcpy(m_status.wFilament1Color, strPtr);
					else
						m_status.wFilament1Color[0] = '\0';
				}
				else
					m_status.wFilament1Color[0] = '\0';

				if(*m_status.wFilament2SerialNumber != '-' && strlen(m_status.wFilament2SerialNumber) > 4)
				{
					strPtr = filamentColorIdToStr(m_status.wFilament2SerialNumber[4]);
					if(strPtr)
						strcpy(m_status.wFilament2Color, strPtr);
					else
						m_status.wFilament2Color[0] = '\0';
				}
				else
					m_status.wFilament2Color[0] = '\0';
				break;

			//case 'x': break; // unused
			//case 'y': break; // unused

			case 'z': // z offset
				if(sscanf(str, "z:%d", &m_status.zOffset))
					m_status.zOffsetSet = true;
				break;

			// case 'A' to 'F' unused

			case 'G':
				// info on last print and filament used?
				findJsonVal(str, "LastUsed", m_status.GLastUsed, sizeof(m_status.GLastUsed));
				break;

			// case 'H' to 'K' unused

			case 'L': // Lifetime timers, L:xx,ml,el,lt
				//   xx - unknown, set to 1
				//   ml - machine lifetime power on time (minutes)
				//   el - extruder lifetime power on time (minutes) (print time)
				//   lt - last power on time (minutes) (or last print time?) optional
				sscanf(str, "L:%d,%d,%d,%d", 
					&m_status.LExtruderCount,  // just a guess
					&m_status.LPrinterLifetimePowerOnTime_min, 
					&m_status.LExtruderLifetimePowerOnTime_min, 
					&m_status.LPrinterLastPowerOnTime_min); // optional
				break;

			//case 'M': break; // unused
			//case 'N': break; // unused

			case 'O': // target temp?, O:{"nozzle":"xx","bed":"yy"}
				// xx is nozzle target temp in C
				// yy is bed target temp in C
				if(findJsonVal(str, "nozzle", s1, sizeof(s1)))
					m_status.tExtruderTargetTemp_C = atoi(s1); // set by t: if not set here
				if(findJsonVal(str, "bed", s1, sizeof(s1)))
					m_status.OBedTargetTemp_C = atoi(s1);
				break;

			//case 'P' to 'U' unused

			case 'V': // some sort of version
				//V:5.1.5
				//****FixMe, work out what this is
				sscanf(str, "V:%s", m_status.VString);
				break;

			case 'W': // wifi information
				// wifi information, only with mini w?
				// W:{"ssid":"a","bssid":"b","channel":"c","rssiValue":"d","PHY":"e","security":"f"}
				// all are optional
				//  a is ssid
				//  b is bssid
				//  c is channel
				//  d is rssiValue
				//  e is PHY
				//  f is security
				findJsonVal(str, "ssid", m_status.WSSID, sizeof(m_status.WSSID));
				findJsonVal(str, "bssid", m_status.WBSSID, sizeof(m_status.WBSSID));
				findJsonVal(str, "channel", m_status.WChannel, sizeof(m_status.WChannel));
				findJsonVal(str, "rssiValue", m_status.WRssiValue, sizeof(m_status.WRssiValue)); //****FixMe, match N4NetRssiValue
				findJsonVal(str, "PHY", m_status.WPHY, sizeof(m_status.WPHY));
				findJsonVal(str, "security", m_status.WSecurity, sizeof(m_status.WSecurity));
				break;

			case 'X': // Nozzle Info, X:nt,sn,sn2
				//   nt is nozzle type one of 
				//     3, 84
				//       nozzle diameter 0.3 mm
				//     1, 77, 82 
				//       nozzle diameter 0.4 mm
				//     2
				//       nozzle diameter 0.4 mm, dual extruder
				//     54
				//       nozzle diameter 0.6 mm
				//     56 
				//       nozzle diameter 0.8 mm
				//     L, N, H, Q
				//       lazer engraver
				//   sn is serial number in the form xx-xx-xx-xx-xx-xx-yy
				//     where xx is the nozzle serial number
				//     and yy is the total nozzle print time (in minutes)
				//   sn2 is optional second serial number for second nozzle
				sscanf(str, "X:%d,%[^,],%s", &m_status.XNozzleID, m_status.XNozzle1SerialNumber, m_status.XNozzle2SerialNumber);
				m_status.XNozzleDiameter_mm = XYZV3::nozzleIDToDiameter(m_status.XNozzleID);
				m_status.XNozzleIsLaser = XYZV3::nozzleIDIsLaser(m_status.XNozzleID);
				break;

			// case 'Y' to '3' unused

			case '4': // Query IP
				//4:0.0.0.0 or 
				//4:{"wlan":{"ip":"0.0.0.0","ssid":"","channel":"0","MAC":"20::5e:c4:4f:bd"}}
				if(strlen(str) >= 3 && (isdigit(str[1]) || isdigit(str[2])))
					sscanf(str, "4:%s", m_status.N4NetIP);
				else
				{
					char tstr[512];
					if(findJsonVal(str, "wlan", tstr, sizeof(tstr)))
					{
						findJsonVal(tstr, "ip", m_status.N4NetIP, sizeof(m_status.N4NetIP));
						findJsonVal(tstr, "ssid", m_status.N4NetSSID, sizeof(m_status.N4NetSSID));
						findJsonVal(tstr, "channel", m_status.N4NetChan, sizeof(m_status.N4NetChan));
						findJsonVal(tstr, "MAC", m_status.N4NetMAC, sizeof(m_status.N4NetMAC));
						if(findJsonVal(tstr, "rssiValue", s1, sizeof(s1)))
						{
							m_status.N4NetRssiValue = -atoi(s1);
							m_status.N4NetRssiValuePct = rssiToPct(m_status.N4NetRssiValue);
						}
						else
						{
							m_status.N4NetRssiValue = -100;
							m_status.N4NetRssiValuePct = 0;
						}
					}
				}
				break;

			// case '5' to '9' unused

			// end of list
			case '$':
				break;
			// error as in E1$
			case 'E':
				debugPrint(DBG_WARN, "XYZV3::parseStatusSubstring got error: %s", str);
				break;

			default:
				debugPrint(DBG_WARN, "XYZV3::parseStatusSubstring unknown string: %s", str);
				break;
			}
		}
		else
			debugPrint(DBG_WARN, "XYZV3::parseStatusSubstring unknown string: %s", str);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::parseStatusSubstring invalid input");
}

// query status, then look for 
// x:yyy ...
// $
// 
// We can query using 'a' to fill in all values
// in that case we zero out the data and look for a '$' to indicate that everything was returned
//
// Otherwise we can query for up to 4 sub values
// in that case only the sub values are returned, on wifi no terminating '$' is sent
void XYZV3::queryStatusStart(bool doPrint, const char *s)
{
	debugPrint(DBG_LOG, "XYZV3::queryStatusStart(%d, %s...)", doPrint, s);

	if(m_useV2Protocol)
	{
		//****RemoveMe, temporary hack to test code out
		// find a better way to integrate this
		if(isWIFI())
			//V2W_CaptureImage();
			V2W_queryStatusStart(doPrint);
		else
			V2S_queryStatusStart(doPrint);
		setState(ACT_SUCCESS);
	}
	else // V3 protocol
	{
		queryStatusSubStateStart(doPrint, s);
		setState(ACT_QS_START);
	}
}

void XYZV3::queryStatusRun()
{
	if(!queryStatusSubStateRun())
		setState(m_actSubState);
}

void XYZV3::queryStatusSubStateStart(bool doPrint, const char *s)
{
	debugPrint(DBG_LOG, "XYZV3::queryStatusSubStateStart(%d, %s...)", doPrint, s);

	if(s && *s !='\0')
	{
		// if asking for all then no need to send anything else
		if(strchr(s, 'a'))
			sprintf(m_qStr, "a");
		else
			strcpy(m_qStr, s);

		m_doPrint = doPrint;
		memset(m_qRes, 0, sizeof(m_qRes));

		m_progress = 0;
		setSubState(ACT_QS_START);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::queryStatusSubStateStart invalid input");
}

bool XYZV3::queryStatusSubStateRun()
{
	const char *buf;
	debugPrint(DBG_LOG, "XYZV3::queryStatusSubStateRun() %d", m_actSubState);

	if(!m_stream 
//		|| !m_stream->isOpen()
		)
		setSubState(ACT_FAILURE);

	switch(m_actSubState)
	{
	case ACT_QS_START: // start
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/query=%s", m_qStr))
			{
				// zero out results, only if updating everything
				//****FixMe, rather than clearing out old data, just keep an update count
				// and provide a 'isNewData() test
				if(m_qStr[0] == 'a')
					memset(&m_status, 0, sizeof(m_status));

				setSubState(ACT_QS_CHECK_FOR_LINE);
			}
			else setSubState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_QS_CHECK_FOR_LINE:
		//****Note, could save a touch of time by polling till buffer is dry
		buf = checkForLine();
		if(*buf)
		{
			if(m_doPrint)
				printf("%s\n", buf);

			// process line
			parseStatusSubstring(buf);

			//****Note, previously we checked for 'j' when requesting all
			// to verify the data as valid. Is that still needed?

			// check for end of message
			if(buf[0] == '$') // end of message
			{
				m_status.isValid = true;
				setSubState(ACT_SUCCESS);
			}
			else if(buf[0] == 'E') // error string like E4$\n
			{
				//****Note, may want to ignore E4$ and E7$
				m_status.isValid = false;
				setSubState(ACT_FAILURE);
				debugPrint(DBG_WARN, "XYZV3::queryStatusSubStateRun recieved error: %s", buf);
			}
			else if(m_qStr[0] != 'a' && isWIFI())
			{
				// on wifi we don't terminate the message with a '$'
				// if we are not quearying all parameters (why george, why)
				bool done = true;
				for(int i=0; i<(int)strlen(m_qStr); i++)
				{
					if(m_qStr[i] == buf[0])
						m_qRes[i] = true;
					done &= m_qRes[i];
				}

				if(done)
				{
					m_status.isValid = true;
					setSubState(ACT_SUCCESS);
				}
			}
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);

		// always update progress
		m_progress = (int)(5.0f + 90.0f * m_timeout.getElapsedTime_pct());
		break;
	default:
		assert(false);
		break;
	}

	// return true if still processing
	return (m_actSubState != ACT_SUCCESS && m_actSubState != ACT_FAILURE);
}

float XYZV3::nozzleIDToDiameter(int id)
{
	switch(id)
	{
	case 3:
	case 84:
		return 0.3f;
	case 1:
	case 2:
	case 77:
	case 82:
		return 0.4f;
	case 54:
		return 0.6f;
	case 56:
		return 0.8f;
	}

	return -1.0f;
}

bool XYZV3::nozzleIDIsLaser(int id)
{
	switch(id)
	{
	case 'L':
	case 'N':
	case 'H':
	case 'Q':
		return true;
	}

	return false;
}

const char* XYZV3::stateCodesToStr(int state, int subState)
{
	static char tstr[512];
	tstr[0] = '\0';

	switch(state)
	{
	case PRINT_INITIAL:
		return "initializing";
	case PRINT_HEATING:
		return "heating";
	case PRINT_PRINTING:
		return "printing";
	case PRINT_CALIBRATING:
		return "calibrating";
	case PRINT_CALIBRATING_DONE:
		return "calibration finished";
	case PRINT_IN_PROGRESS:
		return "printing in progress";
	case PRINT_COOLING_DONE:
		return "print cooling finished";
	case PRINT_COOLING_END:
		return "print cooling end";
	case PRINT_ENDING_PROCESS:
		return "print end process";
	case PRINT_ENDING_PROCESS_DONE:
		return "print end process done";
	case PRINT_JOB_DONE:
		return "print job done";
	case PRINT_NONE:
		return "printer idle";
	case PRINT_STOP:
		return "stop print";
	case PRINT_LOAD_FILAMENT:
		return "loading filament";
	case PRINT_UNLOAD_FILAMENT:
		return "unloading filament";
	case PRINT_AUTO_CALIBRATION:
		return "auto calibrating";
	case PRINT_JOG_MODE:
		return "jog mode";
	case PRINT_FATAL_ERROR:
		return "fatal error";

	case STATE_PRINT_FILE_CHECK:
		sprintf(tstr, "file check (%d)", subState);
		return tstr;
	case STATE_PRINT_LOAD_FIALMENT: //  (substatus 12 14)
		sprintf(tstr, "load filament (%d)", subState);
		return tstr;
	case STATE_PRINT_UNLOAD_FIALMENT: //  (substate 22 24)
		// substate 21 is unload start or heating
		// substate 22 is unload unloading or done, I think
		sprintf(tstr, "unload filament (%d)", subState);
		return tstr;
	case STATE_PRINT_JOG_MODE:
		sprintf(tstr, "jog mode (%d)", subState);
		return tstr;
	case STATE_PRINT_FATAL_ERROR:
		sprintf(tstr, "fatal error (%d)", subState);
		return tstr;
	case STATE_PRINT_HOMING:
		sprintf(tstr, "homing (%d)", subState);
		return tstr;
	case STATE_PRINT_CALIBRATE: //  (substate 41 42 43 44 45 46 47 48 49)
		sprintf(tstr, "calibrating (%d)", subState);
		return tstr;
	case STATE_PRINT_CLEAN_NOZZLE: //  (substate 52)
		sprintf(tstr, "clean nozzle (%d)", subState);
		return tstr;
	case STATE_PRINT_GET_SD_FILE:
		sprintf(tstr, "get sd file (%d)", subState);
		return tstr;
	case STATE_PRINT_PRINT_SD_FILE:
		sprintf(tstr, "print sd file (%d)", subState);
		return tstr;
	case STATE_PRINT_ENGRAVE_PLACE_OBJECT: //  (substate 30)
		sprintf(tstr, "engrave place object (%d)", subState);
		return tstr;
	case STATE_PRINT_ADJUST_ZOFFSET:
		sprintf(tstr, "adjust zoffset (%d)", subState);
		return tstr;
	case PRINT_TASK_PAUSED:
		sprintf(tstr, "print paused (%d)", subState);
		return tstr;
	case PRINT_TASK_CANCELING:
		sprintf(tstr, "print canceled (%d)", subState);
		return tstr;
	case STATE_PRINT_BUSY:
		sprintf(tstr, "busy (%d)", subState);
		return tstr;
	case STATE_PRINT_SCANNER_IDLE:
		sprintf(tstr, "scanner idle (%d)", subState);
		return tstr;
	case STATE_PRINT_SCANNER_RUNNING:
		sprintf(tstr, "scanner running (%d)", subState);
		return tstr;

	default:
		sprintf(tstr, "unknown error (%d:%d)", state, subState);
		return tstr;
	}
}

/*
0010 Print bed heating problem, temp out of range
0016 Cartridge 1 not installed properly. 
0033 Turntable movement abnormalities
0055 inkjet head error 
0056 inkjet data error
0057 Unable to detect extruder, Please reinstall the extruder and reconnect the flat cable, then restart the printer.
5001 Calibration failure
5011 Right camera error
5013 Left camera error
5021 Right scanning laser error
5022 Left scanning laser error
5023 Turntable error during calibration
5031 Storage error during calibration
*/
const char* XYZV3::errorCodeToStr(int code)
{
	switch(code)
	{
	case 0x0000: return "no error";

	case 0x001B: return "M_MACHINE_BUSY";				//        machine buisy
	case 0x001F: return "scanner error";				//        scanner error
	case 0x0020: return "scanner buisy";				//        scanner buisy
	case 0x0101: return "M_THERMAL_HEATER_OUT_TIMER";	// (0011) Extruder 1 heating problem, cant heat to target temp
//	case 0x0003: return "M_THERMAL_BED_OUT_TIMER";		// (0003) Print bed heating problem
	case 0x0102: return "M_THERMAL_BED_OUT_TIMER";		// (0003) Print bed heating problem
	case 0x0103: return "M_THERMAL_HEATER_OUT_CONTROL"; // (0014) Extruder 1 heating problem, temp too high
	case 0x0104: return "M_THERMAL_BED_OUT_CONTROL";	// (0013) Print bed heating problem, temp too high
	case 0x0105: return "L_ERROR_SD_CARD";				// ??? is this (0040) Internal storage error, can not read/write sd card
	case 0x0108: return "M_MACHINE_ERROR_X_AXIS";		// (0030) X-axis movement abnormalities or bad endstop
	case 0x0109: return "M_MACHINE_ERROR_Y_AXIS";		// (0031) Y-axis movement abnormalities or bad endstop
	case 0x010A: return "M_MACHINE_ERROR_Z_AXIS";		// (0032) Z-axis movement abnormalities or bad endstop
	case 0x010B: return "M_FLASHMEMORY_ERROR";			// (0050) Memory error
	case 0x010D: return "L_ERROR_FLASH_RAM";			// (0051) Internal communication error, Reboot the printer
	case 0x010E: return "L_ERROR_NOZZLE_EEPROM";		// (0052) Extruder storage error, Replace the extruder
	case 0x010F: return "L_40W35W_NOZZLE_EEPROM";		// (0054) Incompatible Nozzle
	case 0x0201: return "M_PC_COMMUNICATION_ERROR";		// (0201) Connection error between PC and printer
	case 0x0202: return "M_FW_UPDATE_ERROR";			//        firmware update error
	case 0x0203: return "M_FILAMENT_JAM";				//        filament jam
	case 0x0205: return "M_FILAMENT_WRONG";				//        wrong filament
	case 0x0206: return "M_NO_CASSETTE";				// (0028) Cartridge 1 not installed
	case 0x0207: return "M_CASSETTE_EMPTY";				// (0029, 0060) Cartrige 1 empty
	case 0x0208: return "M_CASSET_EEPROM_WRITE_ERROR";	// (0007) Cartrige 1 chip error (write)
	case 0x0209: return "M_CASSET_EEPROM_READ_ERROR";	// (0008) Cartrige 1 chip error (read)
	case 0x020D: return "Unsuported file version";		//        ???? Only on mini?
	case 0x020C: return "L_ERROR_SD_CARD";				// ??? is this (0040) Internal storage error, can not read/write sd card
	case 0x020F: return "L_FILAMENT_NO_INSTALL";		//
	case 0x0401: return "M_TOP_DOOR_OPEN";				//        top door open
	case 0x0402: return "M_FRONT_DOOR_OPEN";			//        front door open
	case 0x0403: return "M_FILAMENT_LOW";				//
	case 0x0404: return "M_FILAMENT_LOW_TO_EMPTY";		//        filament low or out
	case 0x0405: return "M_FILAMENT_END";				//        filament out

	default: return "---";
	}
}

const char* XYZV3::filamentMaterialTypeToStr(int materialType)
{
	switch(materialType)
	{
	case 41: return "ABS";
	case 46: return "TPE";
	case 47: return "PETG";
	case 50: return "PLA";
	case 51: return "PLA";
	case 54: return "Tough PLA";
	case 56: return "PVA";
	default: return "---";
	}
}

const char* XYZV3::filamentColorIdToStr(int colorId)
{
	switch(colorId)
	{
	case '0': return "Bronze";
	case '1': return "Silver";
	case '2': return "Clear Red";
	case '3': return "Clear";
	case '4': return "Bottle Green";
	case '5': return "Neon Magenta";
	case '6': return "SteelBlue";
	case '7': return "Sun Orange";
	case '8': return "Pearl White";
	case '9': return "Copper";
	case 'A': return "Purple";
	case 'B': return "Blue";
	case 'C': return "Neon Tangerine";
	case 'D': return "Viridity";
	case 'E': return "Olivine";
	case 'F': return "Gold";
	case 'G': return "Green";
	case 'H': return "Neon Green";
	case 'I': return "Snow White";
	case 'J': return "Neon Yellow";
	case 'K': return "Black";
	case 'L': return "Violet";
	case 'M': return "Grape Purple";
	case 'N': return "Purpurine";
	case 'O': return "Clear Yellow";
	case 'P': return "Clear Green";
	case 'Q': return "Clear Tangerine";
	case 'R': return "Red";
	case 'S': return "Cyber Yellow";
	case 'T': return "Tangerine";
	case 'U': return "Clear Blue";
	case 'V': return "Clear Purple";
	case 'W': return "White";
	case 'X': return "Clear Magenta";
	case 'Y': return "Yellow";
	case 'Z': return "Nature";
	default: return "---";
	}
}

int XYZV3::rssiToPct(int rssi)
{
	if(rssi >= -50)
		return 100;
	if(rssi <= -100)
		return 0;
	return (rssi + 100) * 2;
}

// === action commands ===

/*
int runGetTimeout()
{
	if(xyz.isWIFI())
		return  (g_run_act == ACT_PRINT_FILE_MONITOR) ? 3000 : 
				(g_run_act == ACT_PRINT_FILE ||
				 g_run_act == ACT_CONVERT_FILE ||
				 g_run_act == ACT_UPLOAD_FIRMWARE) ? 500 : 500;
	else
		return  (g_run_act == ACT_PRINT_FILE_MONITOR) ? 1000 : 
				(g_run_act == ACT_PRINT_FILE ||
				 g_run_act == ACT_CONVERT_FILE ||
				 g_run_act == ACT_UPLOAD_FIRMWARE) ? 10 : 50;
}
*/

const char* XYZV3::getStateStr()
{
	switch(m_actState)
	{
	case ACT_FAILURE: return "error";
	case ACT_SUCCESS: return "success";

	// Calibrate Bed
	case ACT_CB_START: return "initializing";
	case ACT_CB_START_SUCCESS: return "initializing";
	case ACT_CB_HOME: return "homing";
	case ACT_CB_ASK_LOWER: return "lower detector";
	case ACT_CB_LOWERED: return "calibrating";
	case ACT_CB_CALIB_START: return "calibrating";
	case ACT_CB_CALIB_RUN: return "calibrating";
	case ACT_CB_ASK_RAISE: return "raise detector";
	case ACT_CB_RAISED: return "finishing";
	case ACT_CB_COMPLETE: return "finishing";

	// Clean Nozzle
	case ACT_CL_START: return "initializing";
	case ACT_CL_START_SUCCESS: return "initializing";
	case ACT_CL_WARMUP_COMPLETE: return "warming up";
	case ACT_CL_CLEAN_NOZLE: return "clean nozle";
	case ACT_CL_FINISH: return "finishing";
	case ACT_CL_COMPLETE: return "finishing";

	// Home printer
	case ACT_HP_START: return "initializing";
	case ACT_HP_START_SUCCESS: return "initializing";
	case ACT_HP_HOME_COMPLETE: return "homing";

	// Jog Printer
	case ACT_JP_START: return "initializing";
	case ACT_JP_START_SUCCESS: return "initializing";
	case ACT_JP_JOG_COMPLETE: return "jogging";

	// Load Fillament
	case ACT_LF_START: return "initializing";
	case ACT_LF_START_SUCCESS: return "initializing";
	case ACT_LF_HEATING: return "heating";
	case ACT_LF_LOADING: return "loading";
	case ACT_LF_WAIT_LOAD: return "check nozle";
	case ACT_LF_LOAD_FINISHED: return "finishing";
	case ACT_LF_LOAD_COMPLETE: return "finishing";

	// Unload Fillament
	case ACT_UF_START: return "initializing";
	case ACT_UF_START_SUCCESS: return "initializing";
	case ACT_UF_HEATING: return "heating";
	case ACT_UF_UNLOADING: return "unloading";
	case ACT_UF_UNLOAD_COMPLETE: return "finishing";
	// only get here if cancel button pressed
	case ACT_UF_CANCEL: return "canceling";
	case ACT_UF_CANCEL_COMPLETE: return "canceling";

	// convert file
	case ACT_CF_START: return "initializing";
	case ACT_CF_COMPLETE: return "finishing";

	// print file
	case ACT_PF_START: return "initializing";
	case ACT_PF_SEND: return "uploading file";
	case ACT_PF_SEND_PROCESS: return "uploading file";
	case ACT_PF_COMPLETE: return "finishing";

	// upload firmware
	case ACT_FW_START: return "initializing";
	case ACT_FW_SEND_PROCESS: return "uploading firmware";
	case ACT_FW_COMPLETE: return "finishing";

	// query status
	case ACT_QS_START: return "initializing";
	case ACT_QS_CHECK_FOR_LINE: return "checking for lines";

	// simple command
	case ACT_SC_START: return "initializing";
	case ACT_SC_GET_ZOFFSET: return "waiting on z-offset value";
	case ACT_SC_GET_ENDCOM: return "waiting on ok";
	case ACT_SC_COMPLETE: return "finishing";

	default: 
		assert(false);
		return "unknown";
	}
}

void XYZV3::setState(int /*ActState*/ state, float timeout_s, bool needsMachineState)
{
	// drain any leftover data if end of state
	if(m_actState != state && (state == ACT_SUCCESS || state == ACT_FAILURE))
		endMessage();

	m_actState = state;

	// force progress bar to 100% on exit
	if(state == ACT_FAILURE || state == ACT_SUCCESS)
		m_progress = 100;

	// get default if not specified
	if(timeout_s < 0)
		timeout_s = (m_stream) ? m_stream->getDefaultTimeout() : 5.0f;
	m_timeout.setTimeout_s(timeout_s);

	// need to update machine state?
	m_needsMachineState = needsMachineState;
	if(m_needsMachineState)
		queryStatusSubStateStart(false, "j");
}

int /*ActState*/ XYZV3::getState()
{
	return m_actState;
}

void XYZV3::setSubState(int/*ActState*/ subState, float timeout_s)
{
	m_actSubState = subState;

	// get default if not specified
	if(timeout_s < 0)
		timeout_s = (m_stream) ? m_stream->getDefaultTimeout() : 5.0f;
	m_timeout.setTimeout_s(timeout_s);
}

int /*ActState*/ XYZV3::getSubState()
{
	return m_actSubState;
}

bool XYZV3::isInProgress()
{
	return	getState() != ACT_SUCCESS && 
			getState() != ACT_FAILURE;
}

bool XYZV3::isSuccess()
{
	return getState() == ACT_SUCCESS;
}

int XYZV3::getProgressPct()
{
	return m_progress;
}

void XYZV3::doProcess()
{
	debugPrint(DBG_LOG, "XYZV3::doProcess() %d", m_actState);

	if(m_actState != ACT_CF_START && m_actState != ACT_CF_COMPLETE
		&& !m_stream
//		|| !m_stream->isOpen()
		)
		setState(ACT_FAILURE);

	// if in middle of pumping state then don't process
	if(m_needsMachineState)
		if(queryStatusSubStateRun())
			return;

	if(m_actState >= ACT_CB_START && m_actState <= ACT_CB_COMPLETE)
		calibrateBedRun();
	else if(m_actState >= ACT_CL_START && m_actState <= ACT_CL_COMPLETE)
		cleanNozzleRun();
	else if(m_actState >= ACT_HP_START && m_actState <= ACT_HP_HOME_COMPLETE)
		homePrinterRun();
	else if(m_actState >= ACT_JP_START && m_actState <= ACT_JP_JOG_COMPLETE)
		jogPrinterRun();
	else if(m_actState >= ACT_LF_START && m_actState <= ACT_LF_LOAD_COMPLETE)
		loadFilamentRun();
	else if(m_actState >= ACT_UF_START && m_actState <= ACT_UF_UNLOAD_COMPLETE)
		unloadFilamentRun();
	else if(m_actState >= ACT_CF_START && m_actState <= ACT_CF_COMPLETE)
		convertFileRun();
	else if(m_actState >= ACT_PF_START && m_actState <= ACT_PF_COMPLETE)
		printFileRun();
	else if(m_actState >= ACT_FW_START && m_actState <= ACT_FW_COMPLETE)
		uploadFirmwareRun();
	else if(m_actState >= ACT_QS_START && m_actState <= ACT_QS_CHECK_FOR_LINE)
		queryStatusRun();
	else if(m_actState >= ACT_SC_START && m_actState <= ACT_SC_COMPLETE)
		simpleCommandRun();

	// and launch state again
	if(m_needsMachineState)
		queryStatusSubStateStart(false, "j");
}

//--------------

// call to start calibration
void XYZV3::calibrateBedStart()
{
	debugPrint(DBG_LOG, "XYZV3::calibrateBedStart()");
	m_progress = 0;
	setState(ACT_CB_START);
}

// call in loop while true to pump status
void XYZV3::calibrateBedRun()
{
	debugPrint(DBG_LOG, "XYZV3::calibrateBedRun() %d", m_actState);
	const int len = 1024;
	char val[len];

	if(!m_stream 
//		|| !m_stream->isOpen()
		)
		setState(ACT_FAILURE);

	switch(m_actState)
	{
	case ACT_CB_START: // start
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=calibratejr:new"))
				setState(ACT_CB_START_SUCCESS);
			else setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_CB_START_SUCCESS: // wait on success
		if(isWIFI())
			setState(ACT_CB_HOME, 120, isWIFI());
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "start"))
				setState(ACT_CB_HOME, 120, isWIFI());
			else
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(5.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_CB_HOME: // wait for signal to lower detector
		if(!isWIFI() && checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "pressdetector"))
				setState(ACT_CB_ASK_LOWER, 240);
			else
				setState(ACT_FAILURE);
		}
		else if(isWIFI() && checkForState(STATE_PRINT_CALIBRATE, 41))
			setState(ACT_CB_ASK_LOWER, 240);
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(10.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_CB_ASK_LOWER: // ask user to lower detector
		// waiting, nothing to do
		if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(15.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_CB_LOWERED: // notify printer detecotr was lowered
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=calibratejr:detectorok"))
				setState(ACT_CB_CALIB_START);
			else setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
	case ACT_CB_CALIB_START: // wait for calibration to start
		if(isWIFI())
			setState(ACT_CB_CALIB_RUN, 240, isWIFI());
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "processing"))
				setState(ACT_CB_CALIB_RUN, 240, isWIFI());
			else
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(20.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_CB_CALIB_RUN: // wait for calibration to finish
		if(!isWIFI() && checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "ok")) // or stat:fail
				setState(ACT_CB_ASK_RAISE, 240);
			else
				setState(ACT_FAILURE);
		}
		else if(isWIFI() && checkForState(STATE_PRINT_CALIBRATE, 44))
			setState(ACT_CB_ASK_RAISE, 240);
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(25.0f + 65.0f * 4.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_CB_ASK_RAISE: // ask user to raise detector
		// waiting, nothing to do
		if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(90.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_CB_RAISED: // notify printer detector was raised
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=calibratejr:release"))
				setState(ACT_CB_COMPLETE);
			else setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_CB_COMPLETE:
		if(isWIFI())
			setState(ACT_SUCCESS);
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "complete"))
				setState(ACT_SUCCESS);
			else
				setState(ACT_FAILURE);
		}
		else  if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(95.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	default:
		assert(false);
		break;
	}
}

bool XYZV3::calibrateBedPromptToLowerDetector()
{
	return m_actState == ACT_CB_ASK_LOWER;
}

// ask user to lower detector, then call this
void XYZV3::calibrateBedDetectorLowered()
{
	debugPrint(DBG_LOG, "XYZV3::calibrateBedDetectorLowered()");
	assert(m_actState == ACT_CB_ASK_LOWER);

	setState(ACT_CB_LOWERED);
}

bool XYZV3::calibrateBedPromptToRaiseDetector()
{
	return m_actState == ACT_CB_ASK_RAISE;
}

// ask user to raise detector, then call this
void XYZV3::calibrateBedDetectorRaised()
{
	debugPrint(DBG_LOG, "XYZV3::calibrateBedDetectorRaised()");
	assert(m_actState == ACT_CB_ASK_RAISE);

	setState(ACT_CB_RAISED);
}

void XYZV3::cleanNozzleStart()
{
	debugPrint(DBG_LOG, "XYZV3::cleanNozzleStart()");
	m_progress = 0;
	setState(ACT_CL_START);
}

// call in loop while true to pump status
void XYZV3::cleanNozzleRun()
{
	debugPrint(DBG_LOG, "XYZV3::cleanNozzleRun() %d", m_actState);
	const int len = 1024;
	char val[len];

	if(!m_stream 
//		|| !m_stream->isOpen()
		)
		setState(ACT_FAILURE);

	switch(m_actState)
	{
	case ACT_CL_START:
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=cleannozzle:new"))
				setState(ACT_CL_START_SUCCESS);
			else setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_CL_START_SUCCESS:
		if(isWIFI())
			setState(ACT_CL_WARMUP_COMPLETE, 120, isWIFI());
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "start"))
				setState(ACT_CL_WARMUP_COMPLETE, 120, isWIFI());
			else
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(5.0f + 5.0f * m_timeout.getElapsedTime_pct());
	case ACT_CL_WARMUP_COMPLETE:
		if(!isWIFI() && checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "complete")) // or state is PRINT_NONE
				setState(ACT_CL_CLEAN_NOZLE, 240);
			else
				setState(ACT_FAILURE);
		}
		else if(isWIFI() && checkForState(STATE_PRINT_CLEAN_NOZZLE, 52))
			setState(ACT_CL_CLEAN_NOZLE, 240);
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(10.0f + 40.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_CL_CLEAN_NOZLE:
		//printf("clean nozzle with a wire and press enter when finished\n");
		// waiting, nothing to do
		if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(50.0f + 45.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_CL_FINISH:
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=cleannozzle:cancel"))
				setState(ACT_CL_COMPLETE);
			else setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_CL_COMPLETE:
		if(isWIFI())
			setState(ACT_SUCCESS);
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "ok"))
				setState(ACT_SUCCESS);
			else
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(95.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	default:
		assert(false);
		break;
	}
}

bool XYZV3::cleanNozzlePromtToClean()
{
	return m_actState == ACT_CL_CLEAN_NOZLE;
}

void XYZV3::cleanNozzleCancel()
{
	debugPrint(DBG_LOG, "XYZV3::cleanNozzleCancel()");
	// can cancel at any time

	setState(ACT_CL_FINISH);
}

void XYZV3::homePrinterStart()
{
	debugPrint(DBG_LOG, "XYZV3::homePrinterStart()");
	m_progress = 0;
	setState(ACT_HP_START);
}

void XYZV3::homePrinterRun()
{
	debugPrint(DBG_LOG, "XYZV3::homePrinterRun() %d", m_actState);
	const int len = 1024;
	char val[len];

	if(!m_stream 
//		|| !m_stream->isOpen()
		)
		setState(ACT_FAILURE);

	switch(m_actState)
	{
	case ACT_HP_START:
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=home"))
				setState(ACT_HP_START_SUCCESS);
			else setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_HP_START_SUCCESS:
		if(isWIFI())
			setState(ACT_HP_HOME_COMPLETE, 120);
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "start"))
				setState(ACT_HP_HOME_COMPLETE, 120);
			else
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(5.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_HP_HOME_COMPLETE:
		if(!isWIFI() && checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "complete"))
				setState(ACT_SUCCESS);
			else
				setState(ACT_FAILURE);
		}
		else if(isWIFI() && checkForNotState(STATE_PRINT_HOMING, -1))
			setState(ACT_SUCCESS);
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(10.0f + 90.0f * 4.0f * m_timeout.getElapsedTime_pct());
		break;
	default:
		assert(false);
		break;
	}
}

void XYZV3::jogPrinterStart(char axis, int dist_mm)
{
	debugPrint(DBG_LOG, "XYZV3::jogPrinterStart(%c, %d)", axis, dist_mm);

	m_jogAxis = axis;
	m_jogDist_mm = dist_mm;

	m_progress = 0;
	setState(ACT_JP_START);
}

void XYZV3::jogPrinterRun()
{
	debugPrint(DBG_LOG, "XYZV3::jogPrinterRun() %d", m_actState);
	const int len = 1024;
	char val[len];

	if(!m_stream 
//		|| !m_stream->isOpen()
		)
		setState(ACT_FAILURE);

	switch(m_actState)
	{
	case ACT_JP_START:
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=jog:{\"axis\":\"%c\",\"dir\":\"%c\",\"len\":\"%d\"}", m_jogAxis, (m_jogDist_mm < 0) ? '-' : '+', abs(m_jogDist_mm)))
				setState(ACT_JP_START_SUCCESS);
			else setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_JP_START_SUCCESS:
		if(isWIFI())
			setState(ACT_JP_JOG_COMPLETE, 120);
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "start"))
				setState(ACT_JP_JOG_COMPLETE, 120);
			else
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(5.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_JP_JOG_COMPLETE:
		if(!isWIFI() && checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "complete")) // or state is PRINT_NONE
				setState(ACT_SUCCESS);
			else
				setState(ACT_FAILURE);
		}
		// we may fall into idle or error state before we can detect joging state, so just block if joging state detected
		else if(isWIFI() && checkForNotState(STATE_PRINT_JOG_MODE, -1))
			setState(ACT_SUCCESS);
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(10.0f + 90.0f * 4.0f * m_timeout.getElapsedTime_pct());
		break;
	default:
		assert(false);
		break;
	}
}

void XYZV3::loadFilamentStart()
{
	debugPrint(DBG_LOG, "XYZV3::loadFilamentStart()");
	m_progress = 0;
	setState(ACT_LF_START);
}

void XYZV3::loadFilamentRun()
{
	debugPrint(DBG_LOG, "XYZV3::loadFilamentRun() %d", m_actState);
	const int len = 1024;
	char val[len];

	if(!m_stream 
//		|| !m_stream->isOpen()
		)
		setState(ACT_FAILURE);

	switch(m_actState)
	{
	case ACT_LF_START:
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=load:new"))
				setState(ACT_LF_START_SUCCESS);
			else setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_LF_START_SUCCESS:
		if(isWIFI())
			setState(ACT_LF_HEATING, 120);
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "start"))
				setState(ACT_LF_HEATING, 120);
			else
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(5.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_LF_HEATING:
		if(!isWIFI() && checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "heat"))
				setState(ACT_LF_LOADING, 240, isWIFI());
			else
				setState(ACT_FAILURE);
		}
		else if(isWIFI())
			setState(ACT_LF_LOADING, 360, isWIFI());
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(10.0f + 30.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_LF_LOADING:
		if(!isWIFI() && checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "load")) 
				setState(ACT_LF_WAIT_LOAD, 240);
			else
				setState(ACT_FAILURE);
		}
		else if(isWIFI() && checkForState(STATE_PRINT_LOAD_FIALMENT, 12))
			setState(ACT_LF_WAIT_LOAD, 240);
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(40.0f + 30.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_LF_WAIT_LOAD:
		// if user does not hit cancel/done then eventually one of these will be returned
		// checkForJsonState("stat", "fail");
		// checkForJsonState("stat", "complete");
		// or state is PRINT_NONE
		if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(70.0f + 25.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_LF_LOAD_FINISHED:
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=load:cancel"))
				setState(ACT_LF_LOAD_COMPLETE);
			else
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_LF_LOAD_COMPLETE:
		if(isWIFI())
			setState(ACT_SUCCESS);
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "complete"))
				setState(ACT_SUCCESS);
			else
				setState(ACT_FAILURE);
		}
		else
			setState(ACT_FAILURE);
		break;
	default:
		assert(false);
		break;
	}
}

bool XYZV3::loadFilamentPromptToFinish()
{
	return m_actState == ACT_LF_WAIT_LOAD;
}

void XYZV3::loadFilamentCancel()
{
	debugPrint(DBG_LOG, "XYZV3::loadFilamentCancel()");
	// can cancel at any time

	setState(ACT_LF_LOAD_FINISHED);
}

void XYZV3::unloadFilamentStart()
{
	debugPrint(DBG_LOG, "XYZV3::unloadFilamentStart()");
	m_progress = 0;
	setState(ACT_UF_START);
}

void XYZV3::unloadFilamentRun()
{
	debugPrint(DBG_LOG, "XYZV3::unloadFilamentRun() %d", m_actState);
	const int len = 1024;
	char val[len];

	if(!m_stream 
//		|| !m_stream->isOpen()
		)
		setState(ACT_FAILURE);

	switch(m_actState)
	{
	case ACT_UF_START:
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=unload:new"))
				setState(ACT_UF_START_SUCCESS);
			else setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_UF_START_SUCCESS:
		if(isWIFI())
			setState(ACT_UF_HEATING, 120);
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "start"))
				setState(ACT_UF_HEATING, 120);
			else
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(5.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_UF_HEATING:
		if(!isWIFI() && checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "heat")) // could query temp and state with  XYZv3/query=jt
				setState(ACT_UF_UNLOADING, 240, isWIFI());
			else
				setState(ACT_FAILURE);
		}
		else if(isWIFI())
			setState(ACT_UF_UNLOADING, 360, isWIFI());
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(10.0f + 40.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_UF_UNLOADING:
		if(!isWIFI() && checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "unload")) // could query temp and state with  XYZv3/query=jt
				setState(ACT_UF_UNLOAD_COMPLETE, 240);
			else
				setState(ACT_FAILURE);
		}
		else if(isWIFI() && checkForState(STATE_PRINT_UNLOAD_FIALMENT, 22))
			setState(ACT_UF_UNLOAD_COMPLETE, 360);
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(50.0f + 40.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_UF_UNLOAD_COMPLETE:
		if(!isWIFI() && checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "complete")) // or state is PRINT_NONE
				setState(ACT_SUCCESS);
			else
				setState(ACT_FAILURE);
		}
		else if(isWIFI() && checkForNotState(STATE_PRINT_UNLOAD_FIALMENT, 22))
			setState(ACT_SUCCESS);
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(90.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;

	// only get here if cancel button pressed
	case ACT_UF_CANCEL:
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage("XYZv3/action=unload:cancel"))
				setState(ACT_UF_CANCEL_COMPLETE);
			else if(m_timeout.isTimeout())
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		else // loop
			m_progress = (int)(90.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_UF_CANCEL_COMPLETE:
		if(isWIFI())
			setState(ACT_SUCCESS);
		else if(checkForJsonState(val, len))
		{
			if(jsonValEquals(val, "complete"))
				setState(ACT_SUCCESS);
			else
				setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(95.0f + 5.0f * m_timeout.getElapsedTime_pct());
		break;
	default:
		assert(false);
		break;
	}
}

void XYZV3::unloadFilamentCancel()
{
	debugPrint(DBG_LOG, "XYZV3::unloadFilamentCancel()");
	// can cancel at any time

	setState(ACT_UF_CANCEL);
}

void XYZV3::simpleCommandStart(float timeout_s, bool getZOffset, const char *format, ...)
{
	debugPrint(DBG_LOG, "XYZV3::simpleCommandStart()");

	if(format)
	{
		va_list arglist;
		va_start(arglist, format);
		vsnprintf(m_scCommand, sizeof(m_scCommand), format, arglist);
		m_scCommand[sizeof(m_scCommand)-1] = '\0';
		va_end(arglist);

		if(m_scCommand[0])
		{
			m_progress = 0;
			m_timeout_s = timeout_s;
			m_scGetZOffset = getZOffset;
			setState(ACT_SC_START);
		}
		else
		{
			setState(ACT_FAILURE);
			debugPrint(DBG_LOG, "XYZV3::simpleCommandStart bad input");
		}
	}
	else
	{
		setState(ACT_FAILURE);
		debugPrint(DBG_LOG, "XYZV3::simpleCommandStart bad input");
	}
}

void XYZV3::simpleCommandRun()
{
	const char *buf;
	debugPrint(DBG_LOG, "XYZV3::simpleCommandRun() %d", m_actState);
	//const char *val;

	if(!m_stream 
//		|| !m_stream->isOpen()
		)
		setState(ACT_FAILURE);

	switch(m_actState)
	{
	case ACT_SC_START:
		if(serialCanSendMessage())
		{
			startMessage();
			if(serialSendMessage(m_scCommand))
			{
				if(m_scGetZOffset)
					setState(ACT_SC_GET_ZOFFSET, m_timeout_s);
				else
					setState(ACT_SC_GET_ENDCOM, m_timeout_s);
			}
			else setState(ACT_FAILURE);
		}
		else if(m_timeout.isTimeout())
			setSubState(ACT_FAILURE);
		break;
	case ACT_SC_GET_ZOFFSET:
		buf = checkForLine();
		if(*buf)
		{
			m_status.zOffset = atoi(buf);
			m_status.zOffsetSet = true;
			setState(ACT_SC_GET_ENDCOM, m_timeout_s);
		}
		else if(m_timeout.isTimeout())
			setState(ACT_FAILURE);
		else // loop
			m_progress = (int)(5.0f + 25.0f * m_timeout.getElapsedTime_pct());
		break;
	case ACT_SC_GET_ENDCOM:
		if(isWIFI()) // if WiFi, no return value
			setState(ACT_SUCCESS);
		else 
		{
			buf = checkForLine();
			if(*buf)
			{
				if(0 == strcmp("ok", buf))
					setState(ACT_SC_COMPLETE, 0.1f);
				else if(buf[0] == 'E')
				{
					debugPrint(DBG_WARN, "XYZV3::simpleCommandRun got error '%s'", buf);
					setState(ACT_SC_COMPLETE, 0.1f);
				}
				else
				{
					debugPrint(DBG_WARN, "XYZV3::simpleCommandRun expected 'ok', got '%s'", buf);
					setState(ACT_FAILURE);
				}
			}
			else if(m_timeout.isTimeout())
				setState(ACT_FAILURE);
			else // loop
				m_progress = (int)(30.0f + 30.0f * m_timeout.getElapsedTime_pct());
		}
		break;
	case ACT_SC_COMPLETE:
		if(isWIFI()) // if WiFi, no return value
			setState(ACT_SUCCESS);
		else
		{
			// check for '$' indicating end of message
			buf = checkForLine();
			if(*buf)
			{
				if(buf[0] == '$')
					setState(ACT_SUCCESS);
				else if(buf[0] == 'E')
				{
					debugPrint(DBG_WARN, "XYZV3::simpleCommandRun $ failed, got error '%s'", buf);
					setState(ACT_FAILURE);
				}
				else
				{
					debugPrint(DBG_WARN, "XYZV3::simpleCommandRun $ failed, instead got '%s'", buf);
					setState(ACT_FAILURE);
				}
			}
			else if(m_timeout.isTimeout())
				setState(ACT_FAILURE);
			else // loop
				m_progress = (int)(50.0f + 70.0f * m_timeout.getElapsedTime_pct());
		}
		break;
	default:
		assert(false);
		break;
	}
}

// === config commands ===

void XYZV3::incrementZOffsetStart(bool up)
{
	debugPrint(DBG_LOG, "XYZV3::incrementZOffsetStart(%d)", up);
	simpleCommandStart(-1, true, "XYZv3/action=zoffset:%s", (up) ? "up" : "down");
}

void XYZV3::getZOffsetStart()
{
	debugPrint(DBG_LOG, "XYZV3::getZOffsetStart()");
	if(!isWIFI()) simpleCommandStart(-1, true, "XYZv3/config=zoffset:get");
}

void XYZV3::setZOffsetStart(int offset)
{
	debugPrint(DBG_LOG, "XYZV3::setZOffsetStart(%d)", offset);
	simpleCommandStart(-1, false, "XYZv3/config=zoffset:set[%d]", offset);
}

void XYZV3::restoreDefaultsStart()
{
	debugPrint(DBG_LOG, "XYZV3::restoreDefaultsStart()");
	simpleCommandStart(-1, false, "XYZv3/config=restoredefault:on");
	//****Note, XYZWare sets energy to 3 at this point
}

void XYZV3::setBuzzerStart(bool enable)
{
	debugPrint(DBG_LOG, "XYZV3::setBuzzerStart(%d)", enable);
	simpleCommandStart(-1, false, "XYZv3/config=buzzer:%s", (enable) ? "on" : "off");
}

void XYZV3::setAutoLevelStart(bool enable)
{
	debugPrint(DBG_LOG, "XYZV3::setAutoLevelStart(%d)", enable);
	simpleCommandStart(-1, false, "XYZv3/config=autolevel:%s", (enable) ? "on" : "off");
}

void XYZV3::setLanguageStart(const char *lang)
{
	assert(lang);
	debugPrint(DBG_LOG, "XYZV3::setLanguageStart(%s)", lang);
	if(lang) simpleCommandStart(-1, false, "XYZv3/config=lang:[%s]", lang);
}

// level is 0-9 minutes till lights turn off
// XYZWare sets this to 0,3,6
void XYZV3::setEnergySavingStart(int level)
{
	debugPrint(DBG_LOG, "XYZV3::setEnergySavingStart(%d)", level);
	simpleCommandStart(-1, false, "XYZv3/config=energy:[%d]", level);
}

void XYZV3::sendDisconnectWifiStart()
{
	debugPrint(DBG_LOG, "XYZV3::sendDisconnectWifiStart()");
	simpleCommandStart(-1, false, "XYZv3/config=disconnectap");
}

void XYZV3::sendEngraverPlaceObjectStart()
{
	debugPrint(DBG_LOG, "XYZV3::sendEngraverPlaceObjectStart()");
	simpleCommandStart(-1, false, "XYZv3/config=engrave:placeobject");
}

void XYZV3::setMachineLifeStart(int time_s)
{
	debugPrint(DBG_LOG, "XYZV3::setMachineLifeStart(%d)", time_s);
	simpleCommandStart(-1, false, "XYZv3/config=life:[%d]", time_s);
}

void XYZV3::setMachineNameStart(const char *name)
{
	assert(name);
	debugPrint(DBG_LOG, "XYZV3::setMachineNameStart(%s)", name);
	if(name) simpleCommandStart(-1, false, "XYZv3/config=name:[%s]", name);
}

void XYZV3::setWifiStart(const char *ssid, const char *password, int channel)
{
	debugPrint(DBG_LOG, "XYZV3::setWifiStart(%s, %s, %d)", ssid, password, channel);
	if(ssid && password) simpleCommandStart(25.0f, false, "XYZv3/config=ssid:[%s,%s,%d]", ssid, password, channel);
}

// === upload commands ===

// print a gcode or 3w file, convert as needed
void XYZV3::printFileStart(const char *path)
{
	debugPrint(DBG_LOG, "XYZV3::printFileStart(%s)", path);
	m_progress = 0;

	if(path)
	{
		strncpy(m_filePath, path, MAX_PATH);
		m_filePath[MAX_PATH-1] = '\0';
		m_fileIsTemp = false;
		setState(ACT_PF_START);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::printFileStart invalid input");
}

void XYZV3::printFileRun()
{
	debugPrint(DBG_LOG, "XYZV3::printFileRun() %d", m_actState);

	if(!m_stream 
//		|| !m_stream->isOpen()
		)
		setState(ACT_FAILURE);

	switch(m_actState)
	{
	case ACT_PF_START:
		// if gcode convert to 3w file
		if(isGcodeFile(m_filePath))
		{
			char tpath[MAX_PATH] = "";
			char tfile[MAX_PATH] = "";
			if( GetTempPath(MAX_PATH, tpath) &&
				GetTempFileName(tpath, NULL, 0, tfile))
			{
				if(encryptFile(m_filePath, tfile, -1))
				{
					strcpy(m_filePath, tfile);
					m_fileIsTemp = true;
				}
			}
		}
		// else if  3w just use file directly
		else if(is3wFile(m_filePath))
		{
			//****FixMe, check if file format matches printer
			// if not then decode to temp file and re-encode
		}

		if(m_filePath[0])
			setState(ACT_PF_SEND);
		else setState(ACT_FAILURE);
		break;
	case ACT_PF_SEND:
		if(sendFileInit(m_filePath, true))
			setState(ACT_PF_SEND_PROCESS);
		else
			setState(ACT_FAILURE);
		break;
	case ACT_PF_SEND_PROCESS:
		if(!sendFileProcess())
			setState(ACT_PF_COMPLETE);
		m_progress = (int)(10.0 + 90.0f * getFileUploadPct());
		break;
	case ACT_PF_COMPLETE:
		if(sendFileFinalize())
		{
			// cleanup temp file, if used
			if(m_fileIsTemp)
				remove(m_filePath);

			setState(ACT_SUCCESS);
		}
		else
			setState(ACT_FAILURE);
		break;
	default:
		assert(false);
		break;
	}
}

void XYZV3::cancelPrint()
{
	debugPrint(DBG_LOG, "XYZV3::cancelPrint()");

	if(m_useV2Protocol)
	{
		if(isWIFI())
			V2W_CancelPrint();
		//else ????
		setState(ACT_SUCCESS);
	}
	else
		simpleCommandStart(-1, false, "XYZv3/config=print[cancel]");
}

void XYZV3::pausePrint()
{
	debugPrint(DBG_LOG, "XYZV3::pausePrint()");

	if(m_useV2Protocol)
	{
		if(isWIFI())
			V2W_PausePrint();
		//else ????
		setState(ACT_SUCCESS);
	}
	else
		simpleCommandStart(-1, false, "XYZv3/config=print[pause]");
}

void XYZV3::resumePrint()
{
	debugPrint(DBG_LOG, "XYZV3::resumePrint()");

	if(m_useV2Protocol)
	{
		if(isWIFI())
			V2W_ResumePrint();
		//else ????
		setState(ACT_SUCCESS);
	}
	else
		simpleCommandStart(-1, false, "XYZv3/config=print[resume]");
}

// call when print finished to prep for a new job
// switches from print done to print ready
// be sure old job is off print bed!!!
void XYZV3::readyPrint()
{
	debugPrint(DBG_LOG, "XYZV3::readyPrint()");
	simpleCommandStart(-1, false, "XYZv3/config=print[complete]");
}

//****FixMe, implement these
/*
XYZv3/config=pda:[1591]
XYZv3/config=pdb:[4387]
XYZv3/config=pdc:[7264]
XYZv3/config=pde:[8046]
*/

// print a gcode or 3w file, convert as needed
void XYZV3::uploadFirmwareStart(const char *path)
{
	debugPrint(DBG_LOG, "XYZV3::uploadFirmwareStart(%s)", path);
	m_progress = 0;

	if(path)
	{
		strncpy(m_filePath, path, MAX_PATH);
		m_filePath[MAX_PATH-1] = '\0';
		setState(ACT_FW_START);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::uploadFirmwareStart invalid input");
}

void XYZV3::uploadFirmwareRun()
{
	debugPrint(DBG_LOG, "XYZV3::uploadFirmwareRun() %d", m_actState);

	if(!m_stream 
//		|| !m_stream->isOpen()
		)
		setState(ACT_FAILURE);

	switch(m_actState)
	{
	case ACT_FW_START:
		if(sendFileInit(m_filePath, false))
			setState(ACT_FW_SEND_PROCESS);
		else
			setState(ACT_FAILURE);
		break;
	case ACT_FW_SEND_PROCESS:
		if(!sendFileProcess())
			setState(ACT_PF_COMPLETE);
		m_progress = (int)(10.0 + 90.0f * getFileUploadPct());
		break;
	case ACT_FW_COMPLETE:
		if(sendFileFinalize())
			setState(ACT_SUCCESS);
		else
			setState(ACT_FAILURE);
		break;
	default:
		assert(false);
		break;
	}
}

bool XYZV3::waitForConfigOK()
{
	debugPrint(DBG_LOG, "XYZV3::waitForConfigOK()");

	if(isWIFI())
		return true;

	// else check for ok
	const int len = 1024;
	char buf[len] = "";

	if(m_stream 
//		&& m_stream->isOpen()
		)
	{
		msTimeout timeout(m_stream->getDefaultTimeout());
		do
		{
			if(m_stream->readLine(buf, len, m_useV2Protocol))
				break;
		} 
		while(!timeout.isTimeout() && m_stream && m_stream->isOpen());

		if(*buf)
		{
			if(0 == strcmp("ok", buf))
			{
				//****FixMe, some printers return '$\n\n' after 'ok\n'
				// drain any left over messages
				if(m_stream)
					m_stream->clear();

				return true;
			}
			else if(buf[0] == 'E')
				debugPrint(DBG_WARN, "XYZV3::waitForConfigOK got error instead '%s'", buf);
			else
				debugPrint(DBG_WARN, "XYZV3::waitForConfigOK expected 'ok', got '%s'", buf);
		}
		else
			debugPrint(DBG_WARN, "XYZV3::waitForConfigOK expected 'ok', got nothing");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::waitForConfigOK failed, connection closed");

	//****FixMe, some printers return '$\n\n' after 'ok\n'
	// drain any left over messages
	if(m_stream)
		m_stream->clear();

	return false;
}


bool XYZV3::sendFileInit(const char *path, bool isPrint)
{
	debugPrint(DBG_LOG, "XYZV3::sendFileInit(%s, %d)", path, isPrint);

	assert(!pDat.isPrintActive);

	memset(&pDat, 0, sizeof(pDat));

	// internal options, expose these at some point
	bool saveToSD = false; // set to true to permanently save 3w file to internal SD card
	bool downgrade = false; // set to true to remove version check on firmware?

	bool success = false;

	startMessage();
	if(m_stream && m_stream->isOpen() && path)
	{
		// try to load file from disk
		char *buf = NULL;
		FILE *f = fopen(path, "rb");
		if(f)
		{
			// get file length
			fseek(f, 0, SEEK_END);
			int len = ftell(f);
			if(!isPrint)
				len -= 16; // skip header on firmware
			fseek(f, 0, SEEK_SET);
			if(len > 0)
			{
				buf = new char[len];
				if(buf)
				{
					// firmware has 16 byte header as a string
					char header[17];
					if((isPrint || 16 == fread(header, 1, 16, f)) &&
						len == (int)fread(buf, 1, len, f)) // read whole file into bufer
					{
						// zero terminate header string
						//****Note, only valid if !isPrint
						header[16] = '\0';

						if(m_useV2Protocol)
						{
							//****RemoveMe, temporary hack to test code out
							// find a better way to integrate this
							if(isWIFI())
							{
								if(isPrint)
									V2W_SendFile(buf, len); // v2 wifi/network
								else
								{
									// cant update firmware over wifi
									debugPrint(DBG_WARN, "XYZV3::sendFileInit failure can't upload firmware over wifi");
									assert(false);
								}
							}
							else
							{
								if(isPrint)
									V2S_SendFile(buf, len); // V2 serial (usb)
								else
									V2S_SendFirmware(buf, len);
							}
							pDat.isPrintActive = true;
							success = true;
						}
						else // V3 protocol
						{
							// now we have a buffer, go ahead and start to print it
							pDat.blockSize = (m_status.isValid) ? m_status.oPacketSize : 8192;
							pDat.blockCount = (len + pDat.blockSize - 1) / pDat.blockSize; // round up
							pDat.lastBlockSize = len % pDat.blockSize;
							pDat.curBlock = 0;
							pDat.data = buf;
							pDat.blockBuf = new char[pDat.blockSize + 12];

							if(pDat.blockBuf)
							{
								//if(serialCanSendMessage())
								//{
									if(isPrint)
										serialSendMessage("XYZv3/upload=temp.gcode,%d%s\n", len, (saveToSD) ? ",SaveToSD" : "");
									else
										serialSendMessage("XYZv3/firmware=temp.bin,%d%s\n", len, (downgrade) ? ",Downgrade" : "");
								//}
								//else if(m_timeout.isTimeout())
								//	setSubState(ACT_FAILURE);

								if(waitForConfigOK())
								{
									pDat.isPrintActive = true;
									success = true;
								}
								else
									debugPrint(DBG_WARN, "XYZV3::sendFileInit failed to send data");
							}
							else
								debugPrint(DBG_WARN, "XYZV3::sendFileInit failed to allocate buffer");
						}
					}

					if(!success)
					{
						if(buf)
							delete [] buf;
						buf = NULL;
						pDat.data = NULL;

						if(pDat.blockBuf)
							delete [] pDat.blockBuf;
						pDat.blockBuf = NULL;

						debugPrint(DBG_WARN, "XYZV3::sendFileInit failed");
					}
				}
				else
					debugPrint(DBG_WARN, "XYZV3::sendFileInit failed to allocate buffer");
			}
			else
				debugPrint(DBG_WARN, "XYZV3::sendFileInit failed file has no data");

			// close file if open
			fclose(f);
			f = NULL;
		}
		else
			debugPrint(DBG_WARN, "XYZV3::sendFileInit failed to open file %s", path);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::sendFileInit invalid input");
	endMessage();

	return success;
}

float XYZV3::getFileUploadPct()
{
	debugPrint(DBG_LOG, "XYZV3::getFileUploadPct()");

	if(pDat.isPrintActive && pDat.blockCount > 0)
		return (float)pDat.curBlock/(float)pDat.blockCount;

	return 0;
}

bool XYZV3::sendFileProcess()
{
	debugPrint(DBG_LOG, "XYZV3::sendFileProcess()");

	assert(pDat.isPrintActive);

	if(m_useV2Protocol)
		return false; // false == finished

	bool success = false;
	if(m_stream && m_stream->isOpen() && pDat.data && pDat.blockBuf)
	{
		int i, t;
		int bMax = pDat.blockCount;
		if(bMax > pDat.curBlock + 4)
			bMax = pDat.curBlock + 4;

		for(i=pDat.curBlock; i<bMax; i++)
		{
			int blockLen = (i+1 == pDat.blockCount) ? pDat.lastBlockSize : pDat.blockSize;
			char *tBuf = pDat.blockBuf;

			// block count
			t = swap32bit(i);
			memcpy(tBuf, &t, 4);
			tBuf += 4;

			// data length
			t = swap32bit(blockLen);
			memcpy(tBuf, &t, 4);
			tBuf += 4;

			// data block
			memcpy(tBuf, pDat.data + i*pDat.blockSize, blockLen);
			tBuf += blockLen;

			// end of data
			t = swap32bit(0);
			memcpy(tBuf, &t, 4);
			tBuf += 4;

			// write out in one shot
			m_stream->write(pDat.blockBuf, blockLen + 12);
			success = waitForConfigOK();
			if(!success) // bail on error
			{
				debugPrint(DBG_WARN, "XYZV3::sendFileProcess failed on write");
				break;
			}
		}

		pDat.curBlock = i;
	}
	else
		debugPrint(DBG_WARN, "XYZV3::sendFileProcess invalid input");

	return success;
}

bool XYZV3::sendFileFinalize()
{
	debugPrint(DBG_LOG, "XYZV3::sendFileFinalize()");

	assert(pDat.isPrintActive);

	if(pDat.data)
		delete [] pDat.data;
	pDat.data = NULL;

	if(pDat.blockBuf)
		delete [] pDat.blockBuf;
	pDat.blockBuf = NULL;

	pDat.isPrintActive = false;

	// close out printing
	//if(serialCanSendMessage())
	//{
		bool success = 
			m_useV2Protocol ||
			(serialSendMessage("XYZv3/uploadDidFinish") &&
			waitForConfigOK());
	//}
	//else if(m_timeout.isTimeout())
	//	setSubState(ACT_FAILURE);

	return success;
}

// === file i/o ===

// print a gcode or 3w file, convert as needed
void XYZV3::convertFileStart(const char *inPath, const char *outPath, int infoIdx)
{
	debugPrint(DBG_LOG, "XYZV3::convertFileStart(%s, %s, %d)", inPath, outPath, infoIdx);
	m_progress = 0;

	if(inPath)
	{
		strncpy(m_filePath, inPath, MAX_PATH);
		m_filePath[MAX_PATH-1] = '\0';

		m_fileOutPath[0] = '\0';
		if(outPath)
		{
			strncpy(m_fileOutPath, outPath, MAX_PATH);
			m_fileOutPath[MAX_PATH-1] = '\0';
		}

		m_infoIdx = infoIdx;

		m_fileIsTemp = false;
		setState(ACT_CF_START);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::convertFileStart invalid input");
}

void XYZV3::convertFileRun()
{
	debugPrint(DBG_LOG, "XYZV3::convertFileRun() %d", m_actState);

	// convert file can run without a connection to printer 
	//if(!m_stream || !m_stream->isOpen())
	//	setState(ACT_FAILURE);

	switch(m_actState)
	{
	case ACT_CF_START:
		// if gcode convert to 3w file
		if(isGcodeFile(m_filePath))
		{
			if(encryptFile(m_filePath, m_fileOutPath, m_infoIdx))
				setState(ACT_CF_COMPLETE);
			else setState(ACT_FAILURE);
		}
		// else if  3w just use file directly
		else if(is3wFile(m_filePath))
		{
			if(decryptFile(m_filePath, m_fileOutPath))
				setState(ACT_CF_COMPLETE);
			else setState(ACT_FAILURE);
		}
		else setState(ACT_FAILURE);
		break;
	case ACT_CF_COMPLETE:
		m_progress = 100;
		setState(ACT_SUCCESS);
		break;
	default:
		assert(false);
		break;
	}
}

bool XYZV3::isGcodeFile(const char *path)
{
	debugPrint(DBG_LOG, "XYZV3::isGcodeFile(%s)", path);

	if(path)
	{
		const char *p = strrchr(path, '.');
		if(p)
			return 0 == strcmp(p, ".gcode") ||
				   0 == strcmp(p, ".gco") ||
				   0 == strcmp(p, ".g");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::isGcodeFile invalid input");

	return false;
}

bool XYZV3::is3wFile(const char *path)
{
	debugPrint(DBG_LOG, "XYZV3::is3wFile(%s)", path);

	if(path)
	{
		const char *p = strrchr(path, '.');
		if(p)
			return 0 == strcmp(p, ".3w");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::is3wFile invalid input");

	return false;
}

bool XYZV3::encryptFile(const char *inPath, const char *outPath, int infoIdx)
{
	debugPrint(DBG_LOG, "XYZV3::encryptFile(%s, %s, %d)", (inPath) ? inPath : "", (outPath) ? outPath : "", infoIdx);

	//msTimer t;
	bool success = false;

	// encrypt to currently connected printer
	const XYZPrinterInfo *info = (m_stream && m_stream->isOpen()) ? m_info : NULL; 
	// unless the user requests a particular printer model
	if(infoIdx > -1)
		info = XYZV3::indexToInfo(infoIdx);
#ifdef _DEBUG
	if(!info)
		info = XYZV3::modelToInfo("dv1MX0A000");		// miniMaker
		//info = XYZV3::modelToInfo("dvF100B000");		// 1.0
#endif

	if(info && inPath && *inPath)
	{
		const char *fileNum = info->fileNum;
		bool fileIsV5 = info->fileIsV5;
		bool fileIsZip = info->fileIsZip;

		// open our source file
		FILE *fi = fopen(inPath, "rb");
		if(fi)
		{
			char tPath[MAX_PATH] = "";
			// and write to disk
			FILE *fo = NULL;
			if(outPath && *outPath)
				fo = fopen(outPath, "wb");
			else
			{
				strcpy(tPath, inPath);
				char *ptr = strrchr(tPath, '.');
				if(!ptr)
					ptr = tPath + strlen(tPath);
				sprintf(ptr, ".3w");
				fo = fopen(tPath, "wb");
			}

			// and our destination file
			if(fo)
			{
				//==============================
				// process the source gcode file

				// get file length
				fseek(fi, 0, SEEK_END);
				int gcodeLen = ftell(fi);
				fseek(fi, 0, SEEK_SET);

				// grab gcode file data
				char *gcode = new char[gcodeLen+1];
				if(gcode)
				{
					fread(gcode, 1, gcodeLen, fi);
					gcode[gcodeLen] = '\0';

					// convert G0 commands to G1
					// strip out print time info from header
					// add back in print time info in xyz format
					// work out length of header
					// padd full file to 16 byte boundary buffer
					char *processedBuf = NULL;
					int processHeaderLen = 0;
					int processTotalLen = 0;

					if(processGCode(gcode, gcodeLen, fileNum, &processedBuf, &processHeaderLen, &processTotalLen))
					{
						char *headerBuf = NULL;
						int headerLen = 0;

						if(encryptHeader(processedBuf, processHeaderLen, fileIsV5, &headerBuf, &headerLen))
						{
							char *bodyBuf = NULL;
							int bodyLen = 0;

							if(encryptBody(processedBuf, processTotalLen, fileIsZip, &bodyBuf, &bodyLen))
							{
								if(writeFile(fo, fileIsV5, fileIsZip, headerBuf, headerLen, bodyBuf, bodyLen))
								{
									// yeay, it worked
									success = true;
								}
								else
									debugPrint(DBG_WARN, "XYZV3::encryptFile failed to write body");

								// cleanup
								delete [] bodyBuf;
								bodyBuf = NULL;
							}
							else
								debugPrint(DBG_WARN, "XYZV3::encryptFile failed to encrypt body");

							// cleanup
							delete [] headerBuf;
							headerBuf = NULL;
						}
						else
							debugPrint(DBG_WARN, "XYZV3::encryptFile failed to encrypt header");

						// cleanup
						delete [] processedBuf;
						processedBuf = NULL;
					}
					else
						debugPrint(DBG_WARN, "XYZV3::encryptFile failed to process gcode");

					delete [] gcode;
					gcode = NULL;
				}
				else
					debugPrint(DBG_WARN, "XYZV3::encryptFile failed to allocate buffer");

				// clean up file data
				fclose(fo);
				fo = NULL;
			}
			else
				debugPrint(DBG_WARN, "XYZV3::encryptFile failed to open out file %s", (outPath) ? outPath : tPath);

			fclose(fi);
			fi = NULL;
		}
		else
			debugPrint(DBG_WARN, "XYZV3::encryptFile failed to open in file %s", inPath);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::encryptFile invalid input");

	//debugPrint(DBG_REPORT, "Encrypt File took %0.2f s", t.getElapsedTime_s());
	return success;
}

bool XYZV3::decryptFile(const char *inPath, const char *outPath)
{
	debugPrint(DBG_LOG, "XYZV3::decryptFile(%s, %s)", inPath, outPath);
	//msTimer t;

	bool success = false;

	if(inPath)
	{
		FILE *f = fopen(inPath, "rb");
		if(f)
		{
			char tPath[MAX_PATH];
			// and write to disk
			FILE *fo = NULL;
			if(outPath && *outPath)
				fo = fopen(outPath, "wb");
			else
			{
				strcpy(tPath, inPath);
				char *ptr = strrchr(tPath, '.');
				if(!ptr)
					ptr = tPath + strlen(tPath);
				sprintf(ptr, ".gcode");
				fo = fopen(tPath, "wb");
			}

			if(fo)
			{
				char buf[64];

				// get file length
				fseek(f, 0, SEEK_END);
				int totalLen = ftell(f);
				fseek(f, 0, SEEK_SET);

				// check if this is even a 3w file
				fread(buf, 1, 12, f);
				if(0 == strncmp("3DPFNKG13WTW", buf, strlen("3DPFNKG13WTW")))
				{
					// file format id's
					// id should be 1, what does this mean?
					// file format version is 2 or 5
					fread(buf, 1, 4, f);
					int id = buf[0];
					(void)id; // currently not used

					int fv = buf[1];

					bool fileIsV5 = (fv == 5);

					// offset to zip marker
					int zipOffset = readWord(f);
					fseek(f, zipOffset, SEEK_CUR);

					// zip format marker
					fread(buf, 1, 8, f);
					bool fileIsZip = false;
					if(0 == strncmp("TagEa128", buf, strlen("TagEa128")))
						fileIsZip = true;
					else if(0 == strncmp("TagEJ256", buf, strlen("TagEJ256")))
						fileIsZip = false;
					//else error

					// optional header len
					int headerLen = (fileIsV5) ? readWord(f) : -1;
					(void)headerLen;

					// offset to header
					int headerOffset = readWord(f);

					int offset1 = ftell(f);

					//?? 
					int id2 = (fileIsV5) ? readWord(f) : -1;
					(void)id2;

					int crc32 = readWord(f);

					int offset = headerOffset - (ftell(f) - offset1);
					fseek(f, offset, SEEK_CUR);

					// header potentially goes from here to 8192
					// v5 format stores header len, but v2 files you just have
					// to search for zeros to indicate the end...
					//****Note, header is duplicated in body, for now just skip it
					// you can uncomment this if you want to verify this is the case
//#define PARCE_HEADER
#ifdef PARCE_HEADER
					if(headerLen < 1)
						headerLen = m_bodyOffset - ftell(f);
					char *hbuf = new char[headerLen + 1];
					if(hbuf)
					{
						fread(hbuf, 1, headerLen, f);
						hbuf[headerLen] = '\0';

						// search from end to remove zeros
						while(headerLen > 1)
						{
							if(hbuf[headerLen-1] != 0)
								break;
							headerLen--;
						}

						//****FixMe, V5 files from XYZMaker have unencrypted header
						// but what marks that as the case and how do we detect it
						// for now assume a comment indicates we are not encrypted
						if(hbuf[0] != ';')
						{
							// decrypt header
							struct AES_ctx ctx;
							uint8_t iv[16] = {0}; // 16 zeros
							char *key = "@xyzprinting.com";
							AES_init_ctx_iv(&ctx, key, iv);
							AES_CBC_decrypt_buffer(&ctx, (uint8_t*)hbuf, headerLen);
						}

						// remove any padding from header
						headerLen = pkcs7unpad(hbuf, headerLen);

						//****FixMe, do something with it
						debugPrint(DBG_VERBOSE, hbuf);

						delete [] hbuf;
						hbuf = NULL;
					}
#endif

					// body contains duplicate header and body of gcode file
					// and always starts at 8192 (0x2000)

					fseek(f, m_bodyOffset, SEEK_SET);
					int bodyLen = totalLen - ftell(f);
					int bufLen = bodyLen + 1;
					char *bBuf = new char[bufLen];
					if(bBuf)
					{
						memset(bBuf, 0, bufLen);
						fread(bBuf, 1, bodyLen, f);
						bBuf[bodyLen] = '\0';

						if(crc32 != (int)calcXYZcrc32(bBuf, bodyLen))
							debugPrint(DBG_WARN, "XYZV3::decryptFile crc's don't match!!!");

						if(fileIsZip)
						{
							// allocate a buf to hold the zip file
							char *zbuf = new char[bufLen];
							if(zbuf)
							{
								// decrypt body
								struct AES_ctx ctx;
								uint8_t iv[16] = {0}; // 16 zeros
								const char *key = "@xyzprinting.com";

								int readOffset = 0;
								int writeOffset = 0;
								const int blockLen = 0x2010; // block grows by 16 bytes when pkcs7 padding is applied

								//msTimer t1;
								// decrypt in blocks
								for(readOffset = 0; readOffset < bodyLen; readOffset += blockLen)
								{
									// last block is smaller, so account for it
									int len = ((bodyLen - readOffset) < blockLen) ? (bodyLen - readOffset) : blockLen;

									// reset decrypter every block
									AES_init_ctx_iv(&ctx, key, iv);

									//****FixMe, loop over this in blocks of 1,000
									AES_CBC_decrypt_buffer(&ctx, (uint8_t*)bBuf+readOffset, len);

									// remove any padding from body
									len = pkcs7unpad(bBuf+readOffset, len);

									// and stash in new buf
									memcpy(zbuf+writeOffset, bBuf+readOffset, len);
									writeOffset += len;
								}
								//debugPrint(DBG_REPORT, "CBC Decrypt took %0.2f s", t1.getElapsedTime_s());

								//msTimer t2;
								mz_zip_archive zip;
								memset(&zip, 0, sizeof(zip));
								if(mz_zip_reader_init_mem(&zip, zbuf, writeOffset, 0))
								{
									int numFiles = mz_zip_reader_get_num_files(&zip);
									if(numFiles == 1)
									{
										const int tstr_len = 512;
										char tstr[tstr_len];
										if(mz_zip_reader_get_filename(&zip, 0, tstr, tstr_len))
											debugPrint(DBG_LOG, "XYZV3::decryptFile zip file name '%s'", tstr);

										//****FixMe, replace with mz_zip_reader_extract_iter_new
										size_t size = 0;
										char *tbuf = (char*)mz_zip_reader_extract_to_heap(&zip, 0, &size, 0);
										if(tbuf)
										{
											fwrite(tbuf, 1, size, fo);
											success = true;

											mz_free(tbuf);
											tbuf = NULL;
										}
										else
											debugPrint(DBG_WARN, "XYZV3::decryptFile error %d", zip.m_last_error);
									}
									else
										debugPrint(DBG_WARN, "XYZV3::decryptFile error numfiles is %d", numFiles);

									mz_zip_reader_end(&zip);
								}
								else
									debugPrint(DBG_WARN, "XYZV3::decryptFile error %s", zip.m_last_error);

								delete [] zbuf;
								zbuf = NULL;
								//debugPrint(DBG_REPORT, "Unzip took %0.2f s", t2.getElapsedTime_s());
							}
						}
						else
						{
							// first char in an unencrypted file will be ';'
							// so check if no encrypted, v5 files sometimes are not encrypted
							if(bBuf[0] != ';')
							{
								//msTimer t1;
								// decrypt body
								struct AES_ctx ctx;
								uint8_t iv[16] = {0}; // 16 zeros
								const char *key = "@xyzprinting.com@xyzprinting.com";
								AES_init_ctx_iv(&ctx, key, iv);

								// decrypt in blocks
								const int blockLen = 0x2010;
								for(int readOffset = 0; readOffset < bodyLen; readOffset += blockLen)
								{
									// last block is smaller, so account for it
									int len = ((bodyLen - readOffset) < blockLen) ? (bodyLen - readOffset) : blockLen;

									//****FixMe, loop over this in blocks of 1,000
									AES_ECB_decrypt_buffer(&ctx, (uint8_t*)(bBuf + readOffset), len);
								}
								//debugPrint(DBG_REPORT, "EBC Decrypt took %0.2f s", t1.getElapsedTime_s());
							}

							// remove any padding from body
							bodyLen = pkcs7unpad(bBuf, bodyLen);

							fwrite(bBuf, 1, bodyLen, fo);
							success = true;
						}

						delete [] bBuf;
						bBuf = NULL;
					}
					else
						debugPrint(DBG_WARN, "XYZV3::decryptFile failed to allocate buffer");
				}

				fclose(fo);
				fo = NULL;
			}
			else
				debugPrint(DBG_WARN, "XYZV3::decryptFile failed to open out file %s", (outPath) ? outPath : tPath);

			fclose(f);
		}
		else
			debugPrint(DBG_WARN, "XYZV3::decryptFile failed to open in file %s", inPath);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::decryptFile invalid input");

	//debugPrint(DBG_REPORT, "Decrypt File took %0.2f s", t.getElapsedTime_s());
	return success;
}

//------------------------------------------

//****Note, this does block but only for 1/10th of a second
bool XYZV3::waitForEndCom()
{
	debugPrint(DBG_VERBOSE, "XYZV3::waitForEndCom()");

	// check for '$' indicating end of message
	const int len = 1024;
	char buf[len] = "";

	if(m_stream && m_stream->isOpen())
	{
		msTimeout timeout(0.1f);
		do
		{
			if(m_stream->readLine(buf, len, m_useV2Protocol))
				break;
		} 
		while(!timeout.isTimeout() && m_stream && m_stream->isOpen());

		if(*buf)
		{
			if(buf[0] == '$')
			{
				//****FixMe, some printers return '$\n\n' after 'ok\n'
				// drain any left over messages
				if(m_stream)
					m_stream->clear();

				return true;
				// success
			}
			else if(buf[0] == 'E')
				debugPrint(DBG_WARN, "XYZV3::waitForEndCom $ failed, got error '%s'", buf);
			else
				debugPrint(DBG_WARN, "XYZV3::waitForEndCom $ failed, instead got '%s'", buf);
		}
		else
			debugPrint(DBG_WARN, "XYZV3::waitForEndCom $ failed, got nothing");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::waitForEndCom failed, connection closed");

	//****FixMe, some printers return '$\n\n' after 'ok\n'
	// drain any left over messages
	if(m_stream)
		m_stream->clear();

	return false;
}

const char* XYZV3::checkForLine()
{
	debugPrint(DBG_VERBOSE, "XYZV3::checkForLine()");

	static const int len = 1024;
	static char buf[len]; //****Note, this buffer is overwriten every time you call checkForLine!!!
	*buf = '\0';

	if(m_stream && m_stream->isOpen())
	{
		if(m_stream->readLine(buf, len, m_useV2Protocol))
		{
			return buf;
		}
		else
			debugPrint(DBG_VERBOSE, "XYZV3::checkForLine failed, timed out");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::checkForLine failed, connection closed");

	return "";
}

bool XYZV3::checkForJsonState(char *val, int maxLen)
{
	assert(val);

	const char *key = "stat";
	debugPrint(DBG_VERBOSE, "XYZV3::checkForJsonState(%s)", key);

	if(val && maxLen > 0)
	{
		// return something
		*val = '\0';

		// return will look something like this
		// unload:{"stat":"heat","extemp":"33"}
		const char *buf = checkForLine();
		if(*buf)
		{
			// grab the '$' char
			waitForEndCom();

			// now find key we are looking for in returned sub struct
			// this will skip over any leading key if needed
			if(findJsonVal(buf, key, val, maxLen) && *val)
			{
				debugPrint(DBG_LOG, "XYZV3::checkForJsonState match found");
				return true;
			}
			else
				debugPrint(DBG_WARN, "XYZV3::checkForJsonState expected '%s', got '%s'", key, buf);
		}
		else
			debugPrint(DBG_VERBOSE, "XYZV3::checkForJsonState expected '%s', got nothing", key);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::checkForJsonState invalid input");

	return false;
}

bool XYZV3::jsonValEquals(const char *tVal, const char *val)
{
	debugPrint(DBG_LOG, "XYZV3::jsonValEquals(%s, %s)", tVal, val);

	if(tVal && val)
	{
		// check for exact match
		if(0 == strncmp(val, tVal, strlen(val)))
		{
			debugPrint(DBG_LOG, "XYZV3::jsonValEquals match found");
			return true;
		}
		else
			debugPrint(DBG_WARN, "XYZV3::jsonValEquals expected '%s', got '%s'", val, tVal);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::jsonValEquals invalid input");

	return false;
}

// keys are ".[a-zA-Z].".:
// ie "bob":
// or bill: // no quotes
// or "frank: // no closing quote! probably can ignore this for now
// we assume key is all one word with no spaces
const char* getNextKey(const char *str, char *key, int maxLen)
{
	assert(str);
	assert(key);
	(void)maxLen; //****FixMe, deal with this

	debugPrint(DBG_VERBOSE, "XYZV3::getNextKey(%s, %d)", str, maxLen);

	if(key && maxLen > 0)
	{
		if(str)
		{
			const char *tstr = str;
			char *tkey = key;
			bool isQuote = false;

			// eat opening quote
			if(*tstr == '"')
			{
				tstr++;
				isQuote = true;
			}

			while(*tstr)
			{
				// if quote, skip over string without investigating
				if(isQuote) 
				{
					if(*tstr == '"')
						isQuote = false;
				}
				else if(*tstr == '"')
					isQuote = true;
				else if(!isalnum(*tstr))
					break;

				// if room left
				if(maxLen > 1)
				{
					*tkey = *tstr;
					tkey++;
					maxLen--;
				}
				tstr++;
			}

			// found a key, remove from string and return
			if(*tstr == ':' && maxLen > 1)
			{
				assert(maxLen > 1);
				if(maxLen <= 1)
					debugPrint(DBG_WARN, "XYZV3::getNextKey maxLen too short!");

				// terminate string
				*tkey = '\0';
				// skip over ':' character
				tstr++;

				// remove trailing quote
				int len = strlen(key);
				if(len > 0 && key[len-1] == '"')
					key[len-1] = '\0';
					
				return tstr;
			}
		}
		else
			debugPrint(DBG_WARN, "XYZV3::getNextKey invalid input");

		// no key found, return unmodified string
		*key = '\0';
	}
	else
		debugPrint(DBG_WARN, "XYZV3::getNextKey invalid input");

	return str;
}

//value is string or everything inside of [] or {} or "" untill we hit , or }
// note we can have nested {} brackets, need to skip over children
// need to strip quotes if found, but only if whole object is quoted (not inside [] or {}
const char* getNextVal(const char *str, char *val, int maxLen)
{
	assert(str);
	assert(val);

	debugPrint(DBG_VERBOSE, "XYZV3::getNextVal(%s, %d)", str, maxLen);

	const char *tstr = str;

	if(val && maxLen > 0)
	{
		*val = '\0';

		if(str)
		{
			int mode = 0;
			int bracketCount = 0;
			char *tval = val;

			// remove leading quote
			if(*tstr == '"')
			{
				tstr++;
				mode = 1;
			}

			while(*tstr)
			{
				if(mode == 0) // look for opening " or [ or { end on , or }
				{
					if(*tstr == ',' || *tstr == '}') // end of value
					{
						// { skip over ',' or '}'
						tstr++;
						break;
					}
					else if(*tstr == '"') // start of quote
						mode = 1;
					else if(*tstr == '[') // start of bracket
						mode = 2;
					else if(*tstr == '{') // start of sub element
					{
						bracketCount++;
						mode = 3;
					}
				}
				else if(mode == 1) // look for closing "
				{
					if(*tstr == '"')
						mode = 0;
				}
				else if(mode == 2) // look for closing ]
				{
					if(*tstr == ']')
						mode = 0;
				}
				else if(mode == 3) // look for closing or nested {}
				{
					if(*tstr == '{')
						bracketCount++;
					else if(*tstr == '}')
					{
						bracketCount--;
						if(bracketCount == 0)
							mode = 0;
					}
				}

				if(maxLen > 1)
				{
					*tval = *tstr;
					tval++;
					maxLen--;
				}
				tstr++;
			}

			assert(maxLen > 1);
			if(maxLen <= 1)
				debugPrint(DBG_WARN, "XYZV3::getNextVal maxLen too short!");

			// terminate string
			*tval = '\0';

			// remove trailing quote
			int len = strlen(val);
			if(len > 0 && val[len-1] == '"')
				val[len-1] = '\0';
		}
		else
			debugPrint(DBG_WARN, "XYZV3::getNextVal invalid input");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::getNextVal invalid input");

	return tstr;
}

bool XYZV3::findJsonVal(const char *str, const char *key, char *val, int maxLen)
{
	debugPrint(DBG_VERBOSE, "XYZV3::findJsonVal(%s, %s, %s, %d)", str, key, val, maxLen);

	if(val && maxLen > 0)
	{
		// make sure we return something
		*val = '\0';

		const char *tstr = str;

		const int maxKey = 64, maxVal = 500;
		char tkey[maxKey], tval[maxVal];

		// read in leading key, if exists
		tstr = getNextKey(tstr, tkey, maxKey);
		// just in case check if this is the key we want
		if(0 == strcmp(key, tkey))
		{
			// if it is the value, then return it without searching any farther
			tstr = getNextVal(tstr, tval, maxVal);
			if(*tval)
			{
				const char *s = tval;
				if(*s == '"')
					s++;

				assert(strlen(s) < (size_t)maxLen);
				strncpy(val, s, maxLen);
				val[maxLen - 1] = '\0';

				if(val[strlen(val)-1] == '"')
					val[strlen(val)-1] = '\0';

				debugPrint(DBG_LOG, "XYZV3::findJsonVal found %s:%s", key, val);
				return true;
			}

			// we found the key but no value so early out
			debugPrint(DBG_WARN, "XYZV3::findJsonVal found key but no value found");
			return false;
		}

		// skip leading brace, if exists
		if(*tstr == '{')
			tstr++;

		// try in ernest to find the key/value pair
		while(tstr && *tstr)
		{
			// grab both key and value, even if not a match
			tstr = getNextKey(tstr, tkey, maxKey);
			tstr = getNextVal(tstr, tval, maxVal);
			if(*tkey)
			{
				if(0 == strcmp(key, tkey))
				{
					if(*tval)
					{
						const char *s = tval;
						if(*s == '"')
							s++;

						assert(strlen(s) < (size_t)maxLen);
						strncpy(val, s, maxLen);
						val[maxLen-1] = '\0';

						if(val[strlen(val)-1] == '"')
							val[strlen(val)-1] = '\0';

						debugPrint(DBG_LOG, "XYZV3::findJsonVal found %s:%s", key, val);
						return true;
					}

					// we found the key but no value so early out
					debugPrint(DBG_WARN, "XYZV3::findJsonVal found key but no value found");
					return false;
				}
			}
		}
	}
	else
		debugPrint(DBG_WARN, "XYZV3::findJsonVal invalid input");

	return false;
}

bool XYZV3::checkForState(int state, int substate)
{
	debugPrint(DBG_VERBOSE, "XYZV3::checkForState(%d, %d)", state, substate);

	if(m_status.jPrinterState == state && (substate < 0 || m_status.jPrinterSubState == substate))
	{
		debugPrint(DBG_LOG, "XYZV3::checkForState %d:%d success found stat %d:%d", state, substate, m_status.jPrinterState, m_status.jPrinterSubState);
		return true;
	}
	else
		debugPrint(DBG_WARN, "XYZV3::checkForState %d:%d but stat is %d:%d", state, substate, m_status.jPrinterState, m_status.jPrinterSubState);

	return false;
}

bool XYZV3::checkForNotState(int state, int substate)
{
	debugPrint(DBG_VERBOSE, "XYZV3::checkForNotState(%d, %d)", state, substate);

	if(!(m_status.jPrinterState == state && (substate < 0 || m_status.jPrinterSubState == substate)))
	{
		debugPrint(DBG_LOG, "XYZV3::checkForState not %d:%d success found stat %d:%d", state, substate, m_status.jPrinterState, m_status.jPrinterSubState);
		return true;
	}
	else
		debugPrint(DBG_WARN, "XYZV3::checkForState not %d:%d but stat is %d:%d", state, substate, m_status.jPrinterState, m_status.jPrinterSubState);

	return false;
}

unsigned int XYZV3::swap16bit(unsigned int in)
{
	return ((in >> 8) & 0x00FF) | ((in << 8) & 0xFF00);
}

unsigned int XYZV3::swap32bit(unsigned int in)
{
	return 
		((in >> 24) & 0x000000FF) |
		((in >> 8)  & 0x0000FF00) |
		((in << 8)  & 0x00FF0000) |
		((in << 24) & 0xFF000000) ;
}

// read a word from a file, in big endian format
int XYZV3::readWord(FILE *f)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::readWord()");

	int i = 0;
	if(f)
	{
		fread(&i, 4, 1, f);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::readWord invalid input");

	return swap32bit(i);
}

// write a word to a file, in big endian format
void XYZV3::writeWord(FILE *f, int i)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::writeWord(%d)", i);

	if(f)
	{
		int t = swap32bit(i);
		fwrite(&t, 4, 1, f);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::writeWord invalid input");
}

int XYZV3::readByte(FILE *f)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::readByte()");

	char i = 0;
	if(f)
	{
		//****FixMe, test?
		fread(&i, 1, 1, f);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::readByte invalid input");

	return (int)i;
}

void XYZV3::writeByte(FILE *f, char c)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::writeByte(%d)", c);

	if(f)
	{
		//****FixMe, test?
		fwrite(&c, 1, 1, f);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::writeByte invalid input");
}

void XYZV3::writeRepeatByte(FILE *f, char byte, int count)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::writeRepeatByte(%d, %d", byte, count);

	if(f)
	{
		for(int i=0; i<count; i++)
		{
			fwrite(&byte, 1, 1, f);
		}
	}
	else
		debugPrint(DBG_WARN, "XYZV3::writeRepeatByte invalid input");
}

int XYZV3::roundUpTo16(int in)
{
	return (in + 15) & 0xFFFFFFF0;
}

int XYZV3::pkcs7unpad(char *buf, int len)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::pkcs7unpad()");

	if(buf && len > 0)
	{
		int count = buf[len-1];
		if(count > 0 && count <= 16)
			buf[len-count] = '\0';

		return len - count;
	}
	else
		debugPrint(DBG_WARN, "XYZV3::pkcs7unpad invalid input");

	return len;
}

//****Note, expects buf to have room to be padded out
int XYZV3::pkcs7pad(char *buf, int len)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::pkcs7pad()");

	if(buf && len > 0)
	{
		// force padding even if we are on a byte boundary
		int newLen = roundUpTo16(len+1);
		int count = newLen - len;

		if(count > 0 && count <= 16)
		{
			for(int i=0; i<count; i++)
			{
				buf[len+i] = (char)count;
			}
		}

		return newLen;
	}
	else
		debugPrint(DBG_WARN, "XYZV3::pkcs7pad invalid input");

	return len;
}

// XYZ's version of CRC32, does not seem to match the standard algorithms
unsigned int XYZV3::calcXYZcrc32(char *buf, int len)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::calcXYZcrc32()");

	static const unsigned int hashTable[] = { 
		0, 1996959894, 3993919788, 2567524794, 124634137, 1886057615, 3915621685,
		2657392035, 249268274, 2044508324, 3772115230, 2547177864, 162941995, 
		2125561021, 3887607047, 2428444049, 498536548, 1789927666, 4089016648, 
		2227061214, 450548861, 1843258603, 4107580753, 2211677639, 325883990, 
		1684777152, 4251122042, 2321926636, 335633487, 1661365465, 4195302755, 
		2366115317, 997073096, 1281953886, 3579855332, 2724688242, 1006888145, 
		1258607687, 3524101629, 2768942443, 901097722, 1119000684, 3686517206, 
		2898065728, 853044451, 1172266101, 3705015759, 2882616665, 651767980, 
		1373503546, 3369554304, 3218104598, 565507253, 1454621731, 3485111705, 
		3099436303, 671266974, 1594198024, 3322730930, 2970347812, 795835527, 
		1483230225, 3244367275, 3060149565, 1994146192, 31158534, 2563907772, 
		4023717930, 1907459465, 112637215, 2680153253, 3904427059, 2013776290, 
		251722036, 2517215374, 3775830040, 2137656763, 141376813, 2439277719, 
		3865271297, 1802195444, 476864866, 2238001368, 4066508878, 1812370925, 
		453092731, 2181625025, 4111451223, 1706088902, 314042704, 2344532202, 
		4240017532, 1658658271, 366619977, 2362670323, 4224994405, 1303535960, 
		984961486, 2747007092, 3569037538, 1256170817, 1037604311, 2765210733, 
		3554079995, 1131014506, 879679996, 2909243462, 3663771856, 1141124467, 
		855842277, 2852801631, 3708648649, 1342533948, 654459306, 3188396048, 
		3373015174, 1466479909, 544179635, 3110523913, 3462522015, 1591671054, 
		702138776, 2966460450, 3352799412, 1504918807, 783551873, 3082640443, 
		3233442989, 3988292384, 2596254646, 62317068, 1957810842, 3939845945, 
		2647816111, 81470997, 1943803523, 3814918930, 2489596804, 225274430, 
		2053790376, 3826175755, 2466906013, 167816743, 2097651377, 4027552580, 
		2265490386, 503444072, 1762050814, 4150417245, 2154129355, 426522225, 
		1852507879, 4275313526, 2312317920, 282753626, 1742555852, 4189708143, 
		2394877945, 397917763, 1622183637, 3604390888, 2714866558, 953729732, 
		1340076626, 3518719985, 2797360999, 1068828381, 1219638859, 3624741850, 
		2936675148, 906185462, 1090812512, 3747672003, 2825379669, 829329135, 
		1181335161, 3412177804, 3160834842, 628085408, 1382605366, 3423369109, 
		3138078467, 570562233, 1426400815, 3317316542, 2998733608, 733239954, 
		1555261956, 3268935591, 3050360625, 752459403, 1541320221, 2607071920, 
		3965973030, 1969922972, 40735498, 2617837225, 3943577151, 1913087877, 
		83908371, 2512341634, 3803740692, 2075208622, 213261112, 2463272603, 
		3855990285, 2094854071, 198958881, 2262029012, 4057260610, 1759359992, 
		534414190, 2176718541, 4139329115, 1873836001, 414664567, 2282248934, 
		4279200368, 1711684554, 285281116, 2405801727, 4167216745, 1634467795, 
		376229701, 2685067896, 3608007406, 1308918612, 956543938, 2808555105, 
		3495958263, 1231636301, 1047427035, 2932959818, 3654703836, 1088359270, 
		936918000, 2847714899, 3736837829, 1202900863, 817233897, 3183342108, 
		3401237130, 1404277552, 615818150, 3134207493, 3453421203, 1423857449, 
		601450431, 3009837614, 3294710456, 1567103746, 711928724, 3020668471, 
		3272380065, 1510334235, 755167117 };

	unsigned int num = (unsigned int)-1;

	if(buf && len > 0)
	{
		for (int i = 0; i < len; i++)
			num = num >> 8 ^ hashTable[(num ^ buf[i]) & 255];

		num = (unsigned int)((unsigned long long)num ^ (long long)-1);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::calcXYZcrc32 invalid input");

	return num;
}

const char* XYZV3::readLineFromBuf(const char *buf, char *lineBuf, int lineLen)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::readLineFromBuf()");

	// zero out buffer
	if(lineBuf)
		*lineBuf = '\0';

	if(buf && lineBuf && lineLen > 0)
	{
		if(*buf)
		{
			// search for newline or end of string
			int i = 0;
			while(*buf && *buf != '\n' && *buf != '\r' && i < (lineLen-2))
			{
				lineBuf[i] = *buf;
				buf++;
				i++;
			}

			// scroll past newline
			if(*buf == '\r')
				buf++;
			if(*buf == '\n')
				buf++;

			// tack on newline and terminate string
			lineBuf[i] = '\n';
			i++;
			lineBuf[i] = '\0';
			i++;

			// return poiner to new string
			return buf;
		}
		else
			debugPrint(DBG_VERBOSE, "XYZV3::readLineFromBuf buffer is empty");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::readLineFromBuf invalid input");

	return NULL;
}

bool XYZV3::checkLineIsHeader(const char *lineBuf)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::checkLineIsHeader()");

	if(lineBuf)
	{
		// loop over every char
		while(*lineBuf)
		{
			if(*lineBuf == ';') // if comment symbol then part of header
				return true;
			else if(!isspace(*lineBuf)) // else if regular text part of body
				return false;

			lineBuf++;
		}
	}
	else
		debugPrint(DBG_WARN, "XYZV3::checkLineIsHeader invalid input");

	return true; // else just white space, assume header
}

bool XYZV3::processGCode(const char *gcode, const int gcodeLen, const char *fileNum, char **processedBuf, int *headerLen, int *totalLen)
{
	debugPrint(DBG_VERBOSE, "XYZV3::processGCode()");

	// validate parameters
	if(gcode && gcodeLen > 1 && processedBuf && headerLen && headerLen && totalLen)
	{
		*processedBuf = NULL;
		*headerLen = 0;
		*totalLen = 0;

		const int lineLen = 1024;
		char lineBuf[lineLen];

		// parse header once to get info on print time
		int printTime = 1;
		int totalFacets = 50;
		int totalLayers = 10;
		float totalFilament = 1.0f;

		char *s = NULL;
		const char *tcode = gcode;
		tcode = readLineFromBuf(tcode, lineBuf, lineLen);
		while(*lineBuf)
		{
			//****FixMe, this only really works with cura and xyz files
			// to properly handle slic3r and KISSlicer we need to parse
			// the file and estimate everyting.
			// but is that worth the effort?
			if(checkLineIsHeader(lineBuf))
			{
				// strip out the following parameters
				// and capture info if possible
				// ; filename = temp.3w
				// ; print_time = {estimated time in seconds}
				// ; machine = dv1MX0A000 // change to match current printer
				// ; facets = {totalFacets}
				// ; total_layers = {totalLayers}
				// ; version = 18020109
				// ; total_filament = {estimated filament used in mm}

				// print time
				if(NULL != (s = strstr(lineBuf, "print_time = ")))
					printTime = atoi(s + strlen("print_time = "));
				else if(NULL != (s = strstr(lineBuf, "TIME:")))
					printTime = atoi(s + strlen("TIME:"));

				// total facets
				if(NULL != (s = strstr(lineBuf, "facets = ")))
					totalFacets = atoi(s + strlen("facets = "));

				// total layers
				if(NULL != (s = strstr(lineBuf, "total_layers = ")))
					totalLayers = atoi(s + strlen("total_layers = "));
				else if(NULL != (s = strstr(lineBuf, "LAYER_COUNT:")))
					totalLayers = atoi(s + strlen("LAYER_COUNT:"));
				//****FixMe, can look for ;LAYER: x as well

				// filament used
				else if(NULL != (s = strstr(lineBuf, "total_filament = ")))
					totalFilament = (float)atof(s + strlen("total_filament = "));
				else if(NULL != (s = strstr(lineBuf, "filament_used = ")))
					totalFilament = (float)atof(s + strlen("filament_used = "));
				else if(NULL != (s = strstr(lineBuf, "filament used = ")))
					totalFilament = (float)atof(s + strlen("filament used = "));
				else if(NULL != (s = strstr(lineBuf, "Filament used: ")))
					totalFilament = 1000.0f * (float)atof(s + strlen("Filament used: ")); // m to mm

				// potential data to be stripped from header

				// nozzle diameter
				//" nozzle_diameter = "

				// layer height
				//" layer_height = "
				//"Layer height: "

				// print speed
				//" speed = "
				//"nspeed_print = "
			}
			//****Note, probably should not early out, some put there header at the bottom
			else
				break;

			tcode = readLineFromBuf(tcode, lineBuf, lineLen);
		}

		// create working buffer with extra room
		int bBufMaxLen = gcodeLen + 1000;
		int bbufOffset = 0;
		char * bBuf = new char[bBufMaxLen];
		if(bBuf)
		{
			// make a fake header to keep printer happy
			// must come first or printer will fail
			bbufOffset += sprintf(bBuf + bbufOffset, "; filename = temp.3w\n");
			bbufOffset += sprintf(bBuf + bbufOffset, "; print_time = %d\n", printTime);
			bbufOffset += sprintf(bBuf + bbufOffset, "; machine = %s\n", fileNum);
			bbufOffset += sprintf(bBuf + bbufOffset, "; facets = %d\n", totalFacets);
			bbufOffset += sprintf(bBuf + bbufOffset, "; total_layers = %d\n", totalLayers);
			bbufOffset += sprintf(bBuf + bbufOffset, "; version = 18020109\n");
			bbufOffset += sprintf(bBuf + bbufOffset, "; total_filament = %0.2f\n", totalFilament);

			// potential data to add to header
			/*
			; nozzle_diameter = 0.40
			; layer_height = 0.30
			; support_material = 0
			; support_material_extruder = 1
			; extruder_filament = 50.15:0.00
			; extruder = 1
			; filamentid = 50,50,
			; materialid = 0,
			; fill_density = 0.10
			; raft_layers = 0
			; support_density = 0.15
			; shells = 2
			; speed = 35.000
			; brim_width = 0.000
			; dimension = 20.00:20.00:0.65
			; fill_pattern = rectilinear
			; perimeter_speed = 15.000
			; small_perimeter_speed = 10.000
			; bridge_speed = 10.000
			; travel_speed = 60.000
			; retract_speed = 15
			; retract_length = 4.000
			; first_layer_speed = 5.000
			; Call_From_APP = XYZware 2.1.26.1
			; speed_limit_open = 1
			; Total computing time = 0.000 sec. 
			; threads = 1
			*/

			bool isHeader = true;
			bool wasHeader = true;
			int headerEnd = 0;

			// for each line in gcode file
			tcode = gcode;
			tcode = readLineFromBuf(tcode, lineBuf, lineLen);
			while(*lineBuf)
			{
				wasHeader = isHeader;
				if(isHeader)
					isHeader = checkLineIsHeader(lineBuf);

				if(isHeader)
				{
					// strip out duplicate lines
					if( NULL != strstr(lineBuf, "filename")		|| 
						NULL != strstr(lineBuf, "print_time")	||
						NULL != strstr(lineBuf, "machine")		||
						NULL != strstr(lineBuf, "facets")		||
						NULL != strstr(lineBuf, "total_layers")	||
						NULL != strstr(lineBuf, "version")		||
						NULL != strstr(lineBuf, "total_filament") )
					{
						// drop the line on the floor
					}
					else // else just pass it on through
					{
						strcpy(bBuf + bbufOffset, lineBuf);
						bbufOffset += strlen(lineBuf);
					}
				}
				else
				{
					// mark end of header in main buffer
					if(wasHeader)
						headerEnd = bbufOffset;

					// convert G0 to G1
					char *s = strstr(lineBuf, "G0");
					if(!s)
						s = strstr(lineBuf, "g0");
					if(s)
						s[1] = '1';

					// copy to file
					strcpy(bBuf + bbufOffset, lineBuf);
					bbufOffset += strlen(lineBuf);
				}

				tcode = readLineFromBuf(tcode, lineBuf, lineLen);
			}

			*processedBuf = bBuf;
			*headerLen = headerEnd;
			*totalLen = bbufOffset;

			return true;
		}
		else
			debugPrint(DBG_WARN, "XYZV3::processGCode failed to allocate buffer");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::processGCode invalid input");

	return false;
}

bool XYZV3::encryptHeader(const char *gcode, int gcodeLen, bool fileIsV5, char **headerBuf, int *headerLen)
{
	debugPrint(DBG_VERBOSE, "XYZV3::encryptHeader()");
	bool success = false;

	// validate parameters
	if(gcode && headerBuf && headerLen)
	{
		//==============
		// fix up header

		*headerBuf = NULL;
		*headerLen = 0;

		// padded out to 16 byte boundary
		// and copy so we can encrypt it seperately
		int hLen = roundUpTo16(gcodeLen); 
		char *hBuf = new char[hLen+1];
		if(hBuf)
		{
			memcpy(hBuf, gcode, gcodeLen);

			// don't forget to tag the padding
			pkcs7pad(hBuf, gcodeLen);
			hBuf[hLen] = '\0';

			// encrypt the header in CBC mode
			// it appears that v5 files don't always encrypt
			if(!fileIsV5)
			{
				struct AES_ctx ctx;
				uint8_t iv[16] = {0}; // 16 zeros

				const char *hkey = "@xyzprinting.com";
				AES_init_ctx_iv(&ctx, hkey, iv);
				AES_CBC_encrypt_buffer(&ctx, (uint8_t*)hBuf, hLen);
			}

			*headerBuf = hBuf;
			*headerLen = hLen;

			success = true;
		}
		else
			debugPrint(DBG_WARN, "XYZV3::encryptHeader failed to allocate buffer");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::encryptHeader invalid input");

	return success;
}

bool XYZV3::encryptBody(const char *gcode, int gcodeLen, bool fileIsZip, char **bodyBuf, int *bodyLen)
{
	debugPrint(DBG_VERBOSE, "XYZV3::encryptBody()");
	bool success = false;

	// validate parameters
	if(gcode && bodyBuf && bodyLen)
	{
		//============
		// fix up body

		*bodyBuf = NULL;
		*bodyLen = 0;

		if(fileIsZip)
		{
			//msTimer t1;
			mz_zip_archive zip;
			memset(&zip, 0, sizeof(zip));
			if(mz_zip_writer_init_heap(&zip, 0, 0))
			{
				//****FixMe, break this into loop?
				if(mz_zip_writer_add_mem(&zip, "sample.gcode", gcode, gcodeLen, MZ_DEFAULT_COMPRESSION))
				{
					char *zBuf;
					size_t zBufLen;
					if(mz_zip_writer_finalize_heap_archive(&zip, (void**)&zBuf, &zBufLen))
					{
						// encryption is handled on a block by block basis and adds 16 bytes to each block
						const int blockLen = 0x2000;
						int newLen = zBufLen + (zBufLen / blockLen + 1) * 16;

						//debugPrint(DBG_REPORT, "Zip took %0.2f s", t1.getElapsedTime_s());

						char *bBuf = new char[newLen];
						if(bBuf)
						{
							//msTimer t2;
							// encrypt body
							struct AES_ctx ctx;
							uint8_t iv[16] = {0}; // 16 zeros
							const char *key = "@xyzprinting.com";

							int readOffset = 0;
							int writeOffset = 0;

							// decrypt in blocks
							for(readOffset = 0; readOffset < (int)zBufLen; readOffset += blockLen)
							{
								// last block is smaller, so account for it
								int len = ((zBufLen - readOffset) < blockLen) ? (zBufLen - readOffset) : blockLen;

								// and stash in new buf
								memcpy(bBuf+writeOffset, zBuf+readOffset, len);

								// add padding to body
								len = pkcs7pad(bBuf+writeOffset, len);

								// reset decrypter every block
								AES_init_ctx_iv(&ctx, key, iv);

								//****FixMe, loop over this in blocks of 1,000
								AES_CBC_encrypt_buffer(&ctx, (uint8_t*)bBuf+writeOffset, len);

								writeOffset += len;
							}
							//debugPrint(DBG_REPORT, "CBC Encrypt took %0.2f s", t2.getElapsedTime_s());

							*bodyBuf = bBuf;
							*bodyLen = writeOffset;

							success = true;
						}
						else
							debugPrint(DBG_WARN, "XYZV3::encryptBody failed to allocate z buffer");
					}
					else
						debugPrint(DBG_WARN, "XYZV3::encryptBody failed to finalize zip");
				}
				else
					debugPrint(DBG_WARN, "XYZV3::encryptBody failed to add to zip");

				// clean up zip memory
				mz_zip_writer_end(&zip);
			}
			else
				debugPrint(DBG_WARN, "XYZV3::encryptBody failed to init zip");
		}
		else
		{
			int bLen = roundUpTo16(gcodeLen); 
			char *bBuf = new char[bLen+1];
			if(bBuf)
			{
				memcpy(bBuf, gcode, gcodeLen);

				// don't forget to tag the padding
				pkcs7pad(bBuf, gcodeLen);
				bBuf[bLen] = '\0';

				//msTimer t1;
				// encrypt body using ECB mode
				struct AES_ctx ctx;
				uint8_t iv[16] = {0}; // 16 zeros
				const char *bkey = "@xyzprinting.com@xyzprinting.com";
				AES_init_ctx_iv(&ctx, bkey, iv);

				// decrypt in blocks
				const int blockLen = 0x2010;
				for(int writeOffset = 0; writeOffset < bLen; writeOffset += blockLen)
				{
					// last block is smaller, so account for it
					int len = ((bLen - writeOffset) < blockLen) ? (bLen - writeOffset) : blockLen;

					//****FixMe, loop over this in blocks of 1,000
					AES_ECB_encrypt_buffer(&ctx, (uint8_t*)(bBuf + writeOffset), len);
				}
				//debugPrint(DBG_REPORT, "EBC Encrypt took %0.2f s", t1.getElapsedTime_s());

				*bodyBuf = bBuf;
				*bodyLen = bLen;

				success = true;
			}
			else
				debugPrint(DBG_WARN, "XYZV3::encryptBody failed to allocate buffer");
		}
	}
	else
		debugPrint(DBG_WARN, "XYZV3::encryptBody invalid input");

	return success;
}

bool XYZV3::writeFile(FILE *fo, bool fileIsV5, bool fileIsZip, const char *headerBuf, int headerLen, char *bodyBuf, int bodyLen)
{
	debugPrint(DBG_VERBOSE, "XYZV3::writeFile()");

	if(fo && headerBuf && bodyBuf)
	{
		//==================
		// write header info

		// write file id
		fwrite("3DPFNKG13WTW", 1, strlen("3DPFNKG13WTW"), fo);

		// id, what is this
		writeByte(fo, 1);

		// file format version is 2 or 5
		if(fileIsV5)
			writeByte(fo, 5);
		else
			writeByte(fo, 2);
		
		// pad to 4 bytes
		writeByte(fo, 0);
		writeByte(fo, 0);

		// offset to zip marker
		int pos1 = ftell(fo) + 4; // count from next byte after count
		// force at least 16 bytes of padding, is this needed?
		int zipOffset = roundUpTo16(pos1 + 4684) - pos1;
		writeWord(fo, zipOffset);
		writeRepeatByte(fo, 0, zipOffset);

		// zip format marker
		if(fileIsZip)
			fwrite("TagEa128", 1, strlen("TagEa128"), fo);
		else
			fwrite("TagEJ256", 1, strlen("TagEJ256"), fo);

		// optional header len
		if(fileIsV5)
			writeWord(fo, headerLen);

		// offset to header
		int pos2 = ftell(fo) + 4; // count from next byte after count
		// force at least 16 bytes of padding, is this needed?
		int headerOffset = roundUpTo16(pos2 + 68) - pos2; 
		writeWord(fo, headerOffset);

		// mark current file location
		int offset1 = ftell(fo);

		//?? 
		if(fileIsV5)
			writeWord(fo, 1);

		int crc32 = calcXYZcrc32(bodyBuf, bodyLen);
		writeWord(fo, crc32);

		// zero pad to header offset
		int pad1 = headerOffset - (ftell(fo) - offset1);
		writeRepeatByte(fo, 0, pad1);

		// write encrypted and padded header out
		fwrite(headerBuf, 1, headerLen, fo);

		// mark current file location
		int pad2 = m_bodyOffset - ftell(fo);
		// pad with zeros to start of body
		writeRepeatByte(fo, 0, pad2);

		// write encrypted and padded body out
		fwrite(bodyBuf, 1, bodyLen, fo);

		return true;
	}
	else
		debugPrint(DBG_WARN, "XYZV3::writeFile invalid input");
	
	return false;
}

// === machine info ===

int XYZV3::getInfoCount()
{
	return m_infoArrayLen;
}

const XYZPrinterInfo* XYZV3::indexToInfo(int index)
{
	//debugPrint(DBG_VERBOSE, "XYZV3::indexToInfo(%d)", index);

	if(index >= 0 && index < m_infoArrayLen)
	{
		return &m_infoArray[index];
	}
	else
		debugPrint(DBG_WARN, "XYZV3::indexToInfo index not found %d", index);

	return NULL;
}

const XYZPrinterInfo* XYZV3::modelToInfo(const char *modelNum)
{
	debugPrint(DBG_VERBOSE, "XYZV3::modelToInfo(%s)", modelNum);

	if(modelNum)
	{
		// mini w has two model numbers
		//****FixMe, shoule we do this?
		//if(0 == strcmp(modelNum, "dv1MW0B000"))
		//	modelNum = "dv1MW0A000"; 

		for(int i=0; i<m_infoArrayLen; i++)
		{
			if(0 == strcmp(modelNum, m_infoArray[i].modelNum))
			{
				return &m_infoArray[i];
			}
		}

		debugPrint(DBG_WARN, "XYZV3::modelToInfo model number not found '%s'", modelNum);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::modelToInfo invalid input");

	return NULL;
}

const XYZPrinterInfo* XYZV3::serialToInfo(const char *serialNum)
{
	debugPrint(DBG_VERBOSE, "XYZV3::serialToInfo(%s)", serialNum);

	if(serialNum)
	{
		//****Note, mini w has two model numbers
		// see above

		for(int i=0; i<m_infoArrayLen; i++)
		{
			// first 6 digits of serial number is a machine id
			if(0 == strncmp(serialNum, m_infoArray[i].serialNum, 6))
			{
				return &m_infoArray[i];
			}
		}

		debugPrint(DBG_WARN, "XYZV3::serialToInfo serial number not found '%s'", serialNum);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::serialToInfo invalid input");

	return NULL;
}

const char* XYZV3::serialToName(const char *serialNum)
{
	debugPrint(DBG_VERBOSE, "XYZV3::serialToName(%s)", serialNum);

	if(serialNum)
	{
		const XYZPrinterInfo *inf = XYZV3::serialToInfo(serialNum);
		if(inf)
			return inf->screenName;
		else
			debugPrint(DBG_WARN, "XYZV3::serialToName serial number not found '%s'", serialNum);
	}
	else
		debugPrint(DBG_WARN, "XYZV3::serialToName invalid input");

	return serialNum;
}

bool XYZV3::isWIFI()
{
	return m_stream && m_stream->isWIFI();
}


// experimental v2 protocol

const char* XYZV3::waitForLine()
{
	debugPrint(DBG_VERBOSE, "XYZV3::waitForLine()");

	if(m_stream)
	{
		static const int len = 1024;
		static char buf[len]; //****Note, this buffer is overwriten every time you call waitForLine!!!
		*buf = '\0';

		// wait for M1_OK
		msTimeout timeout(m_stream->getDefaultTimeout());
		do
		{
			// try at least once, even if connection is closed.
			// we may have data in the buffer still
			if(m_stream->readLine(buf, len, m_useV2Protocol))
				return buf;
		} 
		while(!timeout.isTimeout() && m_stream->isOpen());

		if(m_stream->isOpen())
			debugPrint(DBG_VERBOSE, "XYZV3::waitForLine failed, timed out");
		else
		{
			// try once more, we may have data in the buffer still
			if(m_stream->readLine(buf, len, m_useV2Protocol))
				return buf;
			else
				debugPrint(DBG_WARN, "XYZV3::waitForLine failed, connection closed");
		}
	}
	else
		debugPrint(DBG_WARN, "XYZV3::waitForLine failed, invalid connection");

	return "";
}

bool XYZV3::waitForResponse(const char *response)
{
	debugPrint(DBG_LOG, "XYZV3::waitForResponse(%s)", response);

	if(response)
	{
		const char *line = waitForLine();
		if(*line)
		{
			if(NULL != strstr(line, response))
			{
				// success
				return true;
			}
			else
				debugPrint(DBG_WARN, "error: %s", line);
		}
		else
			debugPrint(DBG_WARN, "timeout waiting for %s", response);
	}
	else
		debugPrint(DBG_WARN, "bad input");

	return false;
}

int XYZV3::waitForInt(const char *response)
{
	debugPrint(DBG_LOG, "XYZV3::waitForInt(%s)", response);

	if(response)
	{
		const char *line = waitForLine();
		if(*line)
		{
			const char *ret = strstr(line, response);
			if(ret)
			{
				ret += strlen(response);
				return atoi(ret);
			}
			else
				debugPrint(DBG_WARN, "error: %s", line);
		}
		else
			debugPrint(DBG_WARN, "timeout waiting for %s", response);
	}
	else
		debugPrint(DBG_WARN, "bad input");

	return -1;
}

bool XYZV3::fillBuffer(char *buf, int len)
{
	if(m_stream && m_stream->isOpen())
	{
		msTimeout timeout(5000);
		const int block = 1024;
		int offset = 0;
		while(offset < len &&
			!timeout.isTimeout() && 
			m_stream && m_stream->isOpen())
		{
			int count = block;
			// adjust if last block is smaller
			if(count > (len - offset))
				count = (len - offset);

			int read = m_stream->read(buf + offset, count, false);
			if(read == 0)
				break;

			offset += read;
		} 
		return (offset == len);
	}

	return false;
}

const char* XYZV3::findValue(const char *str, const char *key)
{
	if(str &&*str && key && *key)
	{
		const char *t = strstr(str, key);
		if(t)
		{
			t += strlen(key);
			if(*t)
				return t;
		}
	}
	return NULL;
}

// === v2 serial protocol ===

void XYZV3::V2S_queryStatusStart(bool doPrint)
{
	debugPrint(DBG_LOG, "XYZV3::V2S_queryStatusStart(%d)", doPrint);

	const char *buf;
	bool success;

	success = false;
	startMessage();
	if(serialSendMessage("XYZ_@3D:0"))
	{
		buf = waitForLine();
		if(*buf)
		{
			// zero out results, only if updating everything
			//****FixMe, rather than clearing out old data, just keep an update count
			// and provide a 'isNewData() test
			memset(&m_status, 0, sizeof(m_status));
			m_status.isValid = true;
			success = true;
			while(*buf) 
			{
				V2S_parseStatusSubstring(buf, doPrint);
				buf = checkForLine();
			}
		}
		else
			debugPrint(DBG_WARN, "XYZV3::V2S_queryStatusStart failed recieving status 0");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::V2S_queryStatusStart failed sending status 0");
	endMessage();

	// bail early if rest of message failed
	if(success)
	{
		startMessage();
		if(serialSendMessage("XYZ_@3D:5"))
		{
			buf = waitForLine();
			if(*buf)
			{
				m_status.isValid = true;
				while(*buf) 
				{
					V2S_parseStatusSubstring(buf, doPrint);
					buf = checkForLine();
				}
			}
			else
				debugPrint(DBG_WARN, "XYZV3::V2S_queryStatusStart failed recieving status 5");
		}
		else
			debugPrint(DBG_WARN, "XYZV3::V2S_queryStatusStart failed sending status 5");
		endMessage();

		startMessage();
		if(serialSendMessage("XYZ_@3D:6"))
		{
			buf = waitForLine();
			if(*buf)
			{
				m_status.isValid = true;
				while(*buf) 
				{
					V2S_parseStatusSubstring(buf, doPrint);
					buf = checkForLine();
				}
			}
			else
				debugPrint(DBG_WARN, "XYZV3::V2S_queryStatusStart failed recieving status 6");
		}
		else
			debugPrint(DBG_WARN, "XYZV3::V2S_queryStatusStart failed sending status 6");
		endMessage();

		startMessage();
		if(serialSendMessage("XYZ_@3D:7"))
		{
			buf = waitForLine();
			if(*buf)
			{
				m_status.isValid = true;
				while(*buf) 
				{
					V2S_parseStatusSubstring(buf, doPrint);
					buf = checkForLine();
				}
			}
			else
				debugPrint(DBG_WARN, "XYZV3::V2S_queryStatusStart failed recieving status 7");
		}
		else
			debugPrint(DBG_WARN, "XYZV3::V2S_queryStatusStart failed sending status 7");
		endMessage();

		startMessage();
		if(serialSendMessage("XYZ_@3D:8"))
		{
			buf = waitForLine();
			if(*buf)
			{
				m_status.isValid = true;
				while(*buf) 
				{
					V2S_parseStatusSubstring(buf, doPrint);
					buf = checkForLine();
				}
			}
			else
				debugPrint(DBG_WARN, "XYZV3::V2S_queryStatusStart failed recieving status 8");
		}
		else
			debugPrint(DBG_WARN, "XYZV3::V2S_queryStatusStart failed sending status 8");
		endMessage();
	}
}

void XYZV3::V2S_parseStatusSubstring(const char *str, bool doPrint)
{
	debugPrint(DBG_VERBOSE, "XYZV3::V2S_parseStatusSubstring(%s)", str);

	if(str && str[0] != '\0')
	{
		const char *s;

		if(doPrint)
			printf("%s\n", str);

		s = findValue(str, "Welcome:"); //daVinciF11		// 3W file model number
		// ignore output for now

		s = findValue(str, "XYZ_@3D:"); //start
		// ignore output for now

		// a

		s = findValue(str, "BT:"); // b, bed temp
		if(s) m_status.bBedActualTemp_C = atoi(s);

		// c

		s = findValue(str, "WORK_PARSENT:"); // d, print status	percent finished
		if(s) m_status.dPrintPercentComplete = atoi(s);

		s = findValue(str, "WORK_TIME:"); // d elapsed time?
		if(s) m_status.dPrintElapsedTime_m = atoi(s);

		s = findValue(str, "EST_TIME:"); // d remaining time?
		if(s) 
		{
			m_status.dPrintTimeLeft_m = atoi(s);

			// why is this set to this? is it a bad 3w file?
			if(m_status.dPrintTimeLeft_m == 0x04444444)
				m_status.dPrintTimeLeft_m = -1;
		}

		s = findValue(str, "MCH_STATE:"); // e, error status
		if(s) 
		{
			int t = atoi(s);
			// 0x40 is extruder 1
			// 0x20 is extruder 2
			// 0x00 is everything else
			m_status.eErrorStatusHiByte = (t & 0xFF000000) >> 24;
			m_status.eErrorStatus = translateErrorCode(t);
			const char *strPtr = errorCodeToStr(m_status.eErrorStatus);
			if(strPtr)
				strcpy(m_status.eErrorStatusStr, strPtr);
			else
				m_status.eErrorStatusStr[0] = '\0';
		}

		// f-h

		s = findValue(str, "MCH_ID:"); // i, machine serial number convert ? to -
		if(s) strcpy(m_status.iMachineSerialNum, s);

		s = findValue(str, "PRN_STATE:"); // j, printer status
		if(s) 
		{
			m_status.jPrinterState = translateStatus(atoi(s));

			// fill in status string
			const char *strPtr = stateCodesToStr(m_status.jPrinterState, 0);
			if(strPtr)
				strcpy(m_status.jPrinterStateStr, strPtr);
			else
				m_status.jPrinterStateStr[0] = '\0';
		}

		// k

		s = findValue(str, "LANG:"); //0			// l, sort of?
		//****FixMe, this is some sort of an integer list, but what is what
		// ignore output for now

		// m

		s = findValue(str, "PRT_NAME:"); // n, printer name
		if(s) strcpy(m_status.nMachineName, s);

		// o

		s = findValue(str, "MDU:"); // p, printer model number
		if(s)
		{
			strcpy(m_status.pMachineModelNumber, s);
			m_info = XYZV3::modelToInfo(m_status.pMachineModelNumber);
		}

		// q-s

		m_status.tExtruderTargetTemp_C = 0; // not available with v2 protocol?
		s = findValue(str, "ET0:"); // t, extruder temp
		if(s) 
		{
			m_status.tExtruder1ActualTemp_C = atoi(s);
			m_status.tExtruderCount = 1;
		}

		s = findValue(str, "ET1:"); // t, second extruder temp
		if(s)
		{
			m_status.tExtruder2ActualTemp_C = atoi(s);
			m_status.tExtruderCount = 2;
		}
		else
			m_status.tExtruder2ActualTemp_C = 0;

		// u

		s = findValue(str, "OS_V:"); // v, os firmware versions
		if(s) strcpy(m_status.vOsFirmwareVersion, s);

		s = findValue(str, "APP_V:"); // v, app fw version
		if(s) strcpy(m_status.vAppFirmwareVersion, s);

		s = findValue(str, "FW_V:"); // v, firmware version
		if(s) strcpy(m_status.vFirmwareVersion, s);

		// are W1 and EE1 the same thing?
		s = findValue(str, "W1:"); // w & f, fillament info for first spool
		if(s)
		{
			strcpy(m_status.wFilament1SerialNumber, s);

			if(*m_status.wFilament1SerialNumber != '-' && strlen(m_status.wFilament1SerialNumber) > 4)
			{
				const char *tstr = filamentColorIdToStr(m_status.wFilament1SerialNumber[4]);
				if(tstr)
					strcpy(m_status.wFilament1Color, tstr);
				else
					m_status.wFilament1Color[0] = '\0';
			}
			else
				m_status.wFilament1Color[0] = '\0';
		}

		s = findValue(str, "EE1:"); // EE1:5a,41,570000,343141,240000,240000,210,90,5448,4742,30313135,52 // optional last parameter ,0 is illegal fillament
		// ignore output for now

		// are W2 and EE2 the same thing?
		s = findValue(str, "W2:"); // w & f, fillament info for second spool
		if(s) 
		{
			strcpy(m_status.wFilament2SerialNumber, s);

			if(*m_status.wFilament2SerialNumber != '-' && strlen(m_status.wFilament2SerialNumber) > 4)
			{
				const char *tstr = filamentColorIdToStr(m_status.wFilament2SerialNumber[4]);
				if(tstr)
					strcpy(m_status.wFilament2Color, tstr);
				else
					m_status.wFilament2Color[0] = '\0';
			}
			else
				m_status.wFilament1Color[0] = '\0';
		}

		s = findValue(str, "EE2:");
		// ignore output for now

		// x-z

		// A-K

		s = findValue(str, "MCHLIFE:"); // L, lifetime timers
		if(s) 
			m_status.LPrinterLifetimePowerOnTime_min = atoi(s);

		s = findValue(str, "MCHEXDUR_LIFE:"); // L extruder life
		if(s) m_status.LExtruderLifetimePowerOnTime_min = atoi(s);

		// M-Z

		// 0-3

		s = findValue(str, "PRT_IP:"); // 4, printer ip
		if(s) strcpy(m_status.N4NetIP, s);

		// 5-9

		s = findValue(str, "PROTOCOL:"); //2
		// ignore output for now

		// possible return values, but have not seen them yet
		/*
		FIRMWARE_NAME: // older firmware may contain repetier, marlin, sprinter
		Tmp:		// line tmp?
		Length:		// line length?
		Color:		// line color?
		Matrl:		// line material?
		TalLength:	// line total length?
		FSS:		// filament load?
		CART_STATE:	// cartridge status
		JOB_STATE:	// job status
		X:		// L or ?, x offset?
		Y:		// y offset?
		Z:		// z offset?
		E:		// extruder offset?
		SpeedMultiply:	// speed multiply? int value between 25 and 300
		FlowMultiply:	// flow multiply? int value between 50 and 150
		TargetExtr0:	// target temp exdruder 0 
		TargetExtr1:	// target temp extruder 1
		TargetBed:	// target temp bed
		Fanspeed:	// fan voltage
		RequestPause:
		T:		// temp
		T0:		// dual extruder temp?
		B:		// bed temp
		EPR:		// eprom ???
		MTEMP:		// ?? ?? ?? ??
		Error:		// error
		*/
	}
	else
		debugPrint(DBG_WARN, "XYZV3::V2S_parseStatusSubstring invalid input");
}

void XYZV3::V2S_SendFirmware(const char *buf, int len)
{
	debugPrint(DBG_LOG, "XYZV3::V2S_SendFirmware()");

	// this is experimental, don't run unless you know how to recover
	assert(false);

	// pick one, based on machine type and firmware type
	//****FixMe, work out how to do this
	V2S_SendFileHelper(buf, len, V2S_10_FW);
	//V2S_SendFileHelper(buf, len, V2S_11_FW_ENGINE);
	//V2S_SendFileHelper(buf, len, V2S_11_FW_APP);
	//V2S_SendFileHelper(buf, len, V2S_11_FW_OS);
}

void XYZV3::V2S_SendFile(const char *buf, int len)
{
	debugPrint(DBG_LOG, "XYZV3::V2S_SendFile()");

	V2S_SendFileHelper(buf, len, V2S_FILE);
}

void XYZV3::V2S_SendFileHelper(const char *buf, int len, v2sFileMode mode)
{
	debugPrint(DBG_LOG, "XYZV3::V2S_SendFile()");

	bool ret;

	// start print
	startMessage();
	if(serialSendMessage((mode == V2S_FILE) ? "XYZ_@3D:4" : "XYZ_@3D:3")) // 200 ms delay
	{
		// spinn till state is OFFLINE_OK, but not OFFLINE_FAIL, OFFLINE_NO, OFFLINE_NG
		if(mode == V2S_FILE)
			ret = waitForResponse("OFFLINE_OK"); // wait 5 seconds
		else
			ret = waitForResponse("FWOK");
		if(ret)
		{
			// send file info
			if(mode == V2S_FILE)
				ret = serialSendMessage("M1:MyTest,%d,%d.%d.%d,EE1_OK,EE2_OK", len, 1, 0, 0);
			else if(mode == V2S_10_FW)
				ret = serialSendMessage("M1:firmwarelast,%d", len);
			else if(mode == V2S_11_FW_ENGINE)
				ret = serialSendMessage("M2:engine,engine_data.bin,%d", len);
			else if(mode == V2S_11_FW_APP)
				ret = serialSendMessage("M2:app,app_data.zip,%d", len);
			else if(mode == V2S_11_FW_OS)
				ret = serialSendMessage("M2:os,XYZ_Update.zip,%d", len);
			// 1000 ms delay

			if(ret)
			{
				// wat for M1_OK, or possibly M1_FAIL on 1.1 Plus
				//****FixMe, may not even be returned on 1.0 machine
				if(mode == V2S_FILE || mode == V2S_10_FW)
					ret = waitForResponse("M1_OK"); // wait 5 seconds
				else
					ret = waitForResponse("M2_OK"); // wait 5 seconds

				if(ret)
				{
					// prepare to send data in frames
					const char *dataArray = buf;
					int frameLen = 10236;
					int lastFrameLen = len % frameLen;
					int frameCount = len / frameLen;
					int frameNum = 0;

					// in loop
					while (m_stream && m_stream->isOpen() && frameNum <= frameCount)
					{
						int tFrameLen = (frameNum < frameCount) ? frameLen : lastFrameLen;

						// calc a checksum
						int chk = 0;
						for (int i = 0; i < tFrameLen; i++)
						{
							chk = ((unsigned char)dataArray[i]) + chk;
						}

						// byteswap to network byte order
						chk = swap32bit(chk);

						// send data
						m_stream->write(dataArray, tFrameLen);
						// send checksum
						m_stream->write((const char*)&chk, 4);

						// wait for M2_OK or CheckSumOK
						// or if CheckSumFail send again
						// or if OFFLINE_NO then give up
						//****FixMe, check this
						waitForResponse("M2_OK"); // wait 50 ms

						dataArray += frameLen;
						frameNum++;
					}

					// on 1.1 Plus we send down 3 different firmwares
					// we could send all three then ask them all to
					// be installed at once, but for now just install them as we download them
					// all other machines install all firmwares automatically after downloading
					if(mode == V2S_11_FW_ENGINE)
						serialSendMessage("UPDATE_START:1");
					if(mode == V2S_11_FW_APP)
						serialSendMessage("UPDATE_START:2");
					if(mode == V2S_11_FW_OS)
						serialSendMessage("UPDATE_START:4");
				}
				else
					debugPrint(DBG_LOG, "XYZV3::V2S_SendFile failed to recieve message");
			}
			else
				debugPrint(DBG_LOG, "XYZV3::V2S_SendFile failed to send message");
		}
		else
			debugPrint(DBG_LOG, "XYZV3::V2S_SendFile failed to recieve message");
	}
	else
		debugPrint(DBG_LOG, "XYZV3::V2S_SendFile failed to send message");
	endMessage();
}

// === v2 wifi protocol ===

void XYZV3::V2W_queryStatusStart(bool doPrint, const char *s)
{
	debugPrint(DBG_LOG, "XYZV3::V2W_queryStatusStart(%d, %s)", doPrint, s);

	startMessage();
	if(serialSendMessage("{\"command\":2,\"query\":\"%s\"}", s))
	{
		const char *str = waitForLine(); 
		if(*str)
		{
			debugPrint(DBG_LOG, "XYZV3::V2W_queryStatusStart recieved '%s'", str);
			if(doPrint)
				printf("%s\n", str);

			// {"result":0,"command":2,"data":{"p":"dvF110B000","i":"3F11XPGBXTH5320151","v":{"os":"1.1.0.19","app":"1.1.7.3","engine":"N/A"},"w":["W1:--------------","W2:--------------"],"f":[0,0],"t":["--","--"],"j":10,"L":{"m":"4183146537","e":"21785"},"s":{"e":0,"c":0,"j":0,"o":0},"e":2,"b":"--","d":{"message":"No printing job!"},"o":{"p":0,"t":0,"f":0}}}

			char datStr[512], s1[512], s2[512];
			char a[32] = "", b[32] = "";

			if(findJsonVal(str, "data", datStr, sizeof(datStr)))
			{
				// zero out results, only if updating everything
				//****FixMe, rather than clearing out old data, just keep an update count
				// and provide a 'isNewData() test
				memset(&m_status, 0, sizeof(m_status));
				m_status.isValid = true;

				// a

				if(findJsonVal(datStr, "b", s1, sizeof(s1))) // "b":"--", // bed temperature
					m_status.bBedActualTemp_C = atoi(s1);

				// c

				if(findJsonVal(datStr, "d", s1, sizeof(s1))) // "d":{"message":"No printing job!"}, or "d":{"print_percentage":x,"elapsed_time":x,"estimated_time":x}, // print time
				{
					if(findJsonVal(s1, "print_percentage", s2, sizeof(s2)))
						m_status.dPrintPercentComplete = atoi(s2);

					if(findJsonVal(s1, "elapsed_time", s2, sizeof(s2)))
						m_status.dPrintElapsedTime_m = atoi(s2);

					if(findJsonVal(s1, "estimated_time", s2, sizeof(s2)))
					{
						m_status.dPrintTimeLeft_m = atoi(s2);

						// why is this set to this? is it a bad 3w file?
						if(m_status.dPrintTimeLeft_m == 0x04444444)
							m_status.dPrintTimeLeft_m = -1;
					}
				}

				if(findJsonVal(datStr, "e", s1, sizeof(s1))) // "e":2, // error
				{
					int t = atoi(s1);
					// 0x40 is extruder 1
					// 0x20 is extruder 2
					// 0x00 is everything else
					m_status.eErrorStatusHiByte = (t & 0xFF000000) >> 24;
					m_status.eErrorStatus = translateErrorCode(t);

					const char *strPtr = errorCodeToStr(m_status.eErrorStatus);
					if(strPtr)
						strcpy(m_status.eErrorStatusStr, strPtr);
					else
						m_status.eErrorStatusStr[0] = '\0';
				}

				if(findJsonVal(datStr, "f", s1, sizeof(s1))) // "f":[0,0], // fillament left
				{
					m_status.fFilamentSpoolCount =
						sscanf(s1,"[%d,%d]", &m_status.fFilament1Remaining_mm, &m_status.fFilament2Remaining_mm);
				}

				// g-h

				findJsonVal(datStr, "i", m_status.iMachineSerialNum, sizeof(m_status.iMachineSerialNum)); // "i":"3F11XPGBXTH5320151", // machine serial number, convert ? to -

				if(findJsonVal(datStr, "j", s1, sizeof(s1))) // "j":10, // machine state
				{
					m_status.jPrinterState = translateStatus(atoi(s1));

					// fill in status string
					const char *tstr = stateCodesToStr(m_status.jPrinterState, m_status.jPrinterSubState);
					if(tstr)
						strcpy(m_status.jPrinterStateStr, tstr); 
					else 
						m_status.jPrinterStateStr[0] = '\0';
				}

				// k-m

				findJsonVal(datStr, "n", m_status.nMachineName, sizeof(m_status.nMachineName)); // "n":"name" // printer name

				if(findJsonVal(datStr, "o", s1, sizeof(s1))) // "o":{"p":0,"t":0,"f":0}, // printer options
				{
					//****FixMe, work out what this is
					// deal with s1
					//if(findJsonVal(s1, "p", s2, sizeof(s2)))
					//	m_status.oPacketSize = atoi(s2);
					//if(findJsonVal(s1, "t", s2, sizeof(s2)))
					//	m_status.oT = atoi(s2);
					//if(findJsonVal(s1, "f", s2, sizeof(s2)))
					//	m_status.o = atoi(s2);
				}

				if(findJsonVal(datStr, "p", m_status.pMachineModelNumber, sizeof(m_status.pMachineModelNumber))) // "p":"dvF110B000", // module name or printer type
					m_info = XYZV3::modelToInfo(m_status.pMachineModelNumber);

				// q-r

				if(findJsonVal(datStr, "s", s1, sizeof(s1))) // "s":{"e":0,"c":0,"j":0,"o":0},
				{
					//****FixMe, work out what this is
					// deal with s1
					//if(findJsonVal(s1, "e", s2, sizeof(s2)))
					//	m_status.s= atoi(s2);
					//if(findJsonVal(s1, "c", s2, sizeof(s2)))
					//	m_status.s= atoi(s2);
					//if(findJsonVal(s1, "j", s2, sizeof(s2)))
					//	m_status.s= atoi(s2);
					//if(findJsonVal(s1, "o", s2, sizeof(s2)))
					//	m_status.s= atoi(s2);
				}

				if(findJsonVal(datStr, "t", s1, sizeof(s1))) // "t":["--","--"], // extruder temperature(s)
				{
					m_status.tExtruderCount =
						sscanf(s1, "[\"%[^\"]\",\"%[^\"]\"]", a, b);
					m_status.tExtruderTargetTemp_C = 0; // not available with v2 protocol?
					m_status.tExtruder1ActualTemp_C = atoi(a);
					m_status.tExtruder2ActualTemp_C = atoi(b);
				}

				// u

				if(findJsonVal(datStr, "v", s1, sizeof(s1))) // "v":{"os":"1.1.0.19","app":"1.1.7.3","engine":"N/A"}, // fw version(s)
				{
					findJsonVal(s1, "os", m_status.vOsFirmwareVersion, sizeof(m_status.vOsFirmwareVersion)); // "os":"1.1.0.19"
					findJsonVal(s1, "app", m_status.vAppFirmwareVersion, sizeof(m_status.vAppFirmwareVersion)); // "app":"1.1.7.3"
					findJsonVal(s1, "engine", m_status.vFirmwareVersion, sizeof(m_status.vFirmwareVersion)); // "engine":"N/A"
				}
				
				if(findJsonVal(datStr, "w", s1, sizeof(s1))) // "w":["W1:--------------","W2:--------------"], // eeprom
				{
					m_status.wFilamentCount = 
						sscanf(s1, "[\"W1:%[^\"]\",\"W2:%[^\"]\"]", m_status.wFilament1SerialNumber, m_status.wFilament2SerialNumber);

					if(*m_status.wFilament1SerialNumber != '-' && strlen(m_status.wFilament1SerialNumber) > 4)
					{
						const char *tstr = filamentColorIdToStr(m_status.wFilament1SerialNumber[4]);
						if(tstr)
							strcpy(m_status.wFilament1Color, tstr);
						else
							m_status.wFilament1Color[0] = '\0';
					}
					else
						m_status.wFilament1Color[0] = '\0';

					if(*m_status.wFilament2SerialNumber != '-' && strlen(m_status.wFilament2SerialNumber) > 4)
					{
						const char *tstr = filamentColorIdToStr(m_status.wFilament2SerialNumber[4]);
						if(tstr)
							strcpy(m_status.wFilament2Color, tstr);
						else
							m_status.wFilament2Color[0] = '\0';
					}
					else
					{
						m_status.wFilament2Color[0] = '\0';
						if(m_status.wFilamentCount > 1)
							m_status.wFilamentCount = 1;
					}
				}
				
				// x-z

				// A-K

				if(findJsonVal(datStr, "L", s1, sizeof(s1))) // "L":{"m":"4183146537","e":"21785"}, // machine/extruder lifetime timers
				{
					if(findJsonVal(s1, "m", s2, sizeof(s2)))
					{
#ifdef _WIN32
						__int64 t = _atoi64(s2);
#else
						long long t = atoll(s2);
#endif
						m_status.LPrinterLifetimePowerOnTime_min = (int) (t / 60000); // in milliseconds?
					}

					if(findJsonVal(s1, "e", s2, sizeof(s2)))
						m_status.LExtruderLifetimePowerOnTime_min = atoi(s2); // minutes
				}

				// M-Z

				// 0-9
			}
			else
				debugPrint(DBG_WARN, "XYZV3::V2W_queryStatusStart failed to find data");
		}
		else
			debugPrint(DBG_WARN, "XYZV3::V2W_queryStatusStart failed to get response");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::V2W_queryStatusStart failed to send message");
	endMessage();

	//if(0 == strcmp(s, "all") && *m_V2Token)
	if(0 == strcmp(s, "all"))
	{
		startMessage();
		//if(serialSendMessage("{\"command\":4,\"token\":\"%s\"}", m_V2Token))
		if(serialSendMessage("{\"command\":4}"))
		{
			const char *str = waitForLine(); // <  {"result":1,"command":4,"message":"No printing job!"}
			if(*str)
			{
				debugPrint(DBG_LOG, "XYZV3::V2W_queryStatusStart recieved '%s'", str);
				if(doPrint)
					printf("%s\n", str);
				//****FixMe, parse output
			}
			else
				debugPrint(DBG_WARN, "XYZV3::V2W_queryStatusStart failed to get response");
		}
		else
			debugPrint(DBG_WARN, "XYZV3::V2W_queryStatusStart failed to send message");
		endMessage();
	}

}

void XYZV3::V2W_SendFile(const char *buf, int len)
{
	debugPrint(DBG_LOG, "XYZV3::V2W_SendFile()");

	// start print
	startMessage();
	if(serialSendMessage("{\"command\":1,\"fileName\":\"temp.gcode\",\"fileLen\":%d,\"ee1\":\"EE1_OK\",\"ee2\":\"EE2_OK\"}", len))
	{
		if(waitForResponse("START_RECEIVE"))
		{
			// send file
			int tlen = m_stream->write(buf, len);
			if(tlen == len)
			{
				if(serialSendMessage("<EOF>"))
				{
					const char *str = waitForLine();
					if(str)
					{
						// if print job succeeded then we will get a token identifying the job
						if(!findJsonVal(str, "token", m_V2Token, sizeof(m_V2Token)))
							m_V2Token[0] = '\0';

						char tstr[32];
						int result = -1;
						if(findJsonVal(str, "result", tstr, sizeof(tstr)))
							result = atoi(tstr);

						debugPrint(DBG_LOG, "XYZV3::V2W_SendFile result was %d", result);
						/*
						result == 0 // success
						result == 1 // success?
						result == 2 // unsupported file format
					    result == 4 // file checksum error
					    result == 5 // invalid command
					    result == 6 // printer buisy
						*/
					}
					else
						debugPrint(DBG_WARN, "XYZV3::V2W_SendFile failed to recieve status");
				}
				else
					debugPrint(DBG_WARN, "XYZV3::V2W_SendFile failed to send eof");
			}
			else
				debugPrint(DBG_WARN, "XYZV3::V2W_SendFile failed to send body");
		}
		else
			debugPrint(DBG_WARN, "XYZV3::V2W_SendFile failed to recieve ok");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::V2W_SendFile failed to send command");
	endMessage();
}

bool XYZV3::V2W_CaptureImage(const char *path)
{
	debugPrint(DBG_LOG, "XYZV3::V2W_CaptureImage(%s)", path);

	bool ret = false;

	if(path && *path)
	{
		// request image
		startMessage();
		if(serialSendMessage("{\"command\":3}"))
		{
			int len = waitForInt("length\":"); //{"result":0,"command":3,"message":"START_SEND","length":136811}
			endMessage();
			if(len > 0)
			{
				char *buf = new char[len];
				if(buf)
				{
					// ready to recieve
					startMessage();
					if(serialSendMessage("{\"ack\":\"START_RECEIVE\"}"))
					{
						// recieve buffer
						if(fillBuffer(buf, len))
						{
							int res = waitForInt("result\":"); //{"result":0,"command":3,"message":"SEND_FINISH","length":136811}
							if(res == 0)
							{
								FILE *f = fopen(path, "wb");
								if(f)
								{
									fwrite(buf, sizeof(char), len, f);
									fclose(f);

									ret = true; // success
								}
								else
									debugPrint(DBG_WARN, "XYZV3::V2W_CaptureImage failed to open image");
							}
							else
								debugPrint(DBG_WARN, "XYZV3::V2W_CaptureImage failed with bad result %d", res);
						}
						else
							debugPrint(DBG_WARN, "XYZV3::V2W_CaptureImage failed to recieve image");
						endMessage();

					}
					else
						debugPrint(DBG_WARN, "XYZV3::V2W_CaptureImage failed to send data");

					delete [] buf;
				}
				else
					debugPrint(DBG_WARN, "XYZV3::V2W_CaptureImage failed to allocate buffer %d", len);
			}
			else
				debugPrint(DBG_WARN, "XYZV3::V2W_CaptureImage image not ready");
		}
		else
			debugPrint(DBG_WARN, "XYZV3::V2W_CaptureImage failed to send start");
	}
	else
		debugPrint(DBG_WARN, "XYZV3::V2W_CaptureImage invalid path");

	return ret;
}

void XYZV3::V2W_PausePrint()
{
	debugPrint(DBG_LOG, "XYZV3::V2W_PausePrint()");

	// token is returned when print starts, possibly the file name?
	startMessage();
	if(serialSendMessage("{\"command\":6,\"state\":1,\"token\":\"%s\"}", m_V2Token)) // pause
	{
		int result = waitForInt("result\":"); // <  {"result":5,"command":-1,"message":"Command incorrect!"}
		(void)result; //****FixMe, check if we succeedded
		debugPrint(DBG_LOG, "XYZV3::V2W_PausePrint result was %d", result);
		/*
		result == 0 // success
		result == 1 // success?
		result == 2 // unsupported file format
	    result == 4 // file checksum error
	    result == 5 // invalid command
	    result == 6 // printer buisy
		*/
	}
	else
		debugPrint(DBG_WARN, "XYZV3::V2W_PausePrint failed to send message");
	endMessage();
}

void XYZV3::V2W_ResumePrint()
{
	debugPrint(DBG_LOG, "XYZV3::V2W_ResumePrint()");

	// token is returned when print starts, possibly the file name?
	startMessage();
	if(serialSendMessage("{\"command\":6,\"state\":2,\"token\":\"%s\"}", m_V2Token)) // resume
	{
		int result = waitForInt("result\":"); // <  {"result":5,"command":-1,"message":"Command incorrect!"}
		(void)result; //****FixMe, check if we succeedded
		debugPrint(DBG_LOG, "XYZV3::V2W_ResumePrint result was %d", result);
		/*
		result == 0 // success
		result == 1 // success?
		result == 2 // unsupported file format
	    result == 4 // file checksum error
	    result == 5 // invalid command
	    result == 6 // printer buisy
		*/
	}
	else
		debugPrint(DBG_WARN, "XYZV3::V2W_ResumePrint failed to send");
	endMessage();
}

void XYZV3::V2W_CancelPrint()
{
	debugPrint(DBG_LOG, "XYZV3::V2W_CancelPrint()");

	// token is returned when print starts, possibly the file name?
	startMessage();
	if(serialSendMessage("{\"command\":6,\"state\":3,\"token\":\"%s\"}", m_V2Token)) // stop
	{
		int result = waitForInt("result\":"); // <  {"result":5,"command":-1,"message":"Command incorrect!"}
		(void)result; //****FixMe, check if we succeedded
		debugPrint(DBG_LOG, "XYZV3::V2W_CancelPrint result was %d", result);
		/*
		result == 0 // success
		result == 1 // success?
		result == 2 // unsupported file format
	    result == 4 // file checksum error
	    result == 5 // invalid command
	    result == 6 // printer buisy
		*/
	}
	else
		debugPrint(DBG_WARN, "XYZV3::V2W_CancelPrint failed to send");
	endMessage();
}
