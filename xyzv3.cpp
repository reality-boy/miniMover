#include <SDKDDKVer.h>
#include <Windows.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>

#include "timer.h"
#include "aes.h"
#include "serial.h"
#include "xyzv3.h"

#pragma warning(disable:4996) // live on the edge!

//****FixMe, rather than calling clearSerial() we
// should check what data was left over and assert!

const XYZPrinterInfo XYZV3::m_infoArray[m_infoArrayLen] = {
	//modelNum,    fileNum,        serialNum, screenName,				fileIsV5, fileIsZip, comIsV3, length, width, height
	//  older communication protocol
	{"dvF100B000", "daVinciF10",   "3DP01P", "da Vinci 1.0",			false, true, false, 200, 200, 200},
	{"dvF100A000", "daVinciF10",   "3F10AP", "da Vinci 1.0A",			false, true, false, 200, 200, 200},
	{"dvF10SA000", "daVinciF10",   "3S10AP", "da Vinci AIO",			false, true, false, 200, 200, 200},
	{"dvF200B000", "daVinciF20",   "3F20XP", "da Vinci 2.0 Duo",		false, true, false, 200, 150, 200},
	{"dvF200A000", "daVinciF20",   "3F20AP", "da Vinci 2.0A Duo",		false, true, false, 200, 150, 200},

	// new v3 communication protocol
	{"dvF110B000", "daVinciF11",   "3F11XP", "da Vinci 1.1 Plus",		false, true, true, 200, 200, 200}, //???? I'm suspicious of this one, think it belongs above

	{"dv1NX0A000", "dv1NX0A000",   "3FN1XP", "da Vinci Nano",			false, false, true, 120, 120, 120},
	{"dv1MW0A000", "dv1MW0A000",   "3FM1WP", "da Vinci Mini w",			false, false, true, 150, 150, 150},
	{"dv1MX0A000", "dv1MX0A000",   "3FM1XP", "da Vinci miniMaker",		false, false, true, 150, 150, 150},
	{"dv1J00A000", "daVinciJR10",  "??????", "da Vinci Jr. 1.0",		false, false, true, 150, 150, 150},
	{"dv1JW0A000", "daVinciJR10W", "3F1JWP", "da Vinci Jr. 1.0 Wireless",false,false, true, 150, 150, 150},
	{"dv1JS0A000", "dv1JSOA000",   "3F1JSP", "da Vinci Jr. 3in1",		false, false, true, 150, 150, 150},
	{"dv2JW0A000", "daVinciJR20W", "3F2JWP", "da Vinci Jr. 2.0 Mix",	false, false, true, 150, 150, 150},
	{"dv1JA0A000", "dv1JA0A000",   "3F1JAP", "da Vinci Jr. 1.0A",		false, false, true, 175, 175, 175},

	// new v5 file format
	{"dv1JP0A000", "dv1JP0A000",   "3F1JPP", "da Vinci Jr. 1.0 Pro",	true, false, true, 150, 150, 150},
	{"dv1JSOA000", "daVinciJR10S", "3F1JOP", "da Vinci Jr. 3in1 (Open filament)", true, false, true, 150, 150, 150},
	{"dvF1W0A000", "daVinciAW10",  "3F1AWP", "da Vinci 1.0 Pro",		true, false, true, 200, 200, 200},
	{"dvF1WSA000", "daVinciAS10",  "3F1ASP", "da Vinci 1.0 Pro 3in1",	true, false, true, 200, 200, 190},
	{"dv1SW0A000", "dv1SW0A000",   "3F1SWP", "da Vinci 1.0 Super",		true, false, true, 300, 300, 300},

	//  unknown
	//{"dv1CP0A000"},
	//{"dvF110A000"},
	//{"dvF110P000"},
	//{"dvF11SA000"},
	//{"dvF210B000"},
	//{"dvF210A000"},
	//{"dvF210P000"},
};

XYZV3::XYZV3() 
{
	memset(&m_status, 0, sizeof(m_status));
	ghMutex = CreateMutex(NULL, FALSE, NULL);
} 

XYZV3::~XYZV3() 
{
	CloseHandle(ghMutex);
} 

bool XYZV3::connect(int port)
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	// try to find a good default
	if(port < 0)
		port = XYZV3::refreshPortList();

	if(m_serial.openPort(port, 115200))
	{
		m_serial.clearSerial();
		if(updateStatus())
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

void XYZV3::disconnect()
{
	WaitForSingleObject(ghMutex, INFINITE);
	m_serial.closePort();
	ReleaseMutex(ghMutex);
}

bool XYZV3::isConnected()
{
	return m_serial.isOpen() && m_status.isValid;
}

int XYZV3::refreshPortList()
{
	return Serial::queryForPorts("XYZ");
}

int XYZV3::getPortCount()
{
	return Serial::getPortCount();
}

int XYZV3::getPortNumber(int id)
{
	return Serial::getPortNumber(id);
}

const char* XYZV3::getPortName(int id)
{
	return Serial::getPortName(id);
}
bool XYZV3::updateStatus()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		static const int len = 1024;
		char buf[len];
		const char *strPtr = NULL;

		// zero out results
		memset(&m_status, 0, sizeof(m_status));

		m_serial.writeSerial("XYZv3/query=a");

		// only try so many times for the answer
		bool isDone = false;
		float end = msTime::getTime_s() + 2; // wait a second or two
		while(msTime::getTime_s() < end && !isDone)
		{
			if(m_serial.readSerialLine(buf, len))
			{
				int d1 = 0, d2 = 0, d3 = 0;
				char s1[256] = "";

				switch(buf[0])
				{
				case 'b': // heat bed temperature, b:xx - deg C
					sscanf(buf, "b:%d", &m_status.bedTemp_C);
					break;

				case 'c': // Calibration values, c:{x1,x2,x3,x4,x5,x6,x7,x8,x9}
					sscanf(buf, "c:{%d,%d,%d,%d,%d,%d,%d,%d,%d}",
						&m_status.calib[0], 
						&m_status.calib[1], 
						&m_status.calib[2], 
						&m_status.calib[3], 
						&m_status.calib[4], 
						&m_status.calib[5], 
						&m_status.calib[6], 
						&m_status.calib[7], 
						&m_status.calib[8]);
					break;

				case 'd': // print status, d:ps,el,es
					//   ps - print percent complete (0-100?)
					//   el - print elapsed time (minutes)
					//   es - print estimated time left (minutes)
					//   a value of 0,0,0 indicates no job is running
					sscanf(buf, "d:%d,%d,%d", &m_status.printPercentComplete, &m_status.printElapsedTime_m, &m_status.printTimeLeft_m);

					// why is this set to this? is it a bad 3w file?
					if(m_status.printTimeLeft_m == 0x04444444)
						m_status.printTimeLeft_m = -1;

					break;

				case 'e': // error status, e:ec - some sort of string?
					sscanf(buf, "e:%d", &m_status.errorStatus);
					break;

				case 'f': // filament remaining, f:ct,len
					//   ct - how many spools of filiment, 1 for normal printer
					//   len - filament left in millimeters
					sscanf(buf, "f:%d,%d", &d1, &m_status.fillimantRemaining_mm);
					break;

				case 'g': break; // unused

				case 'h': // pla filament loaded, h:x > 0 if pla filament in printer
					// not used with miniMaker
					sscanf(buf, "h:%d", &m_status.isFillamentPLA);
					break;

				case 'i': // machine serial number, i:sn - serial number
					//   note, convert ? characters to - characters when parsing sn
					sscanf(buf, "i:%s", &m_status.machineSerialNum);
					break;

				case 'j': // printer status, j:st,sb
					//   st - status id
					//   sb - substatus id
					sscanf(buf, "j:%d,%d", &m_status.printerStatus, &m_status.printerSubStatus);

					// translate old printer status codes
					switch(m_status.printerStatus)
					{
					case 0:
						m_status.printerStatus = PRINT_INITIAL;
						break;
					case 1:
						m_status.printerStatus = PRINT_HEATING;
						break;
					case 2:
						m_status.printerStatus = PRINT_PRINTING;
						break;
					case 3:
						m_status.printerStatus = PRINT_CALIBRATING;
						break;
					case 4:
						m_status.printerStatus = PRINT_CALIBRATING_DONE;
						break;
					case 5:
						m_status.printerStatus = PRINT_COOLING_DONE;
						break;
					case 6:
						m_status.printerStatus = PRINT_COOLING_END;
						break;
					case 7:
						m_status.printerStatus = PRINT_ENDING_PROCESS;
						break;
					case 8:
						m_status.printerStatus = PRINT_ENDING_PROCESS_DONE;
						break;
					case 9:
						m_status.printerStatus = PRINT_JOB_DONE;
						break;
					case 10:
						m_status.printerStatus = PRINT_NONE;
						break;
					case 11:
						m_status.printerStatus = PRINT_IN_PROGRESS;
						break;
					case 12:
						m_status.printerStatus = PRINT_STOP;
						break;
					case 13:
						m_status.printerStatus = PRINT_LOAD_FILAMENT;
						break;
					case 14:
						m_status.printerStatus = PRINT_UNLOAD_FILAMENT;
						break;
					case 15:
						m_status.printerStatus = PRINT_AUTO_CALIBRATION;
						break;
					case 16:
						m_status.printerStatus = PRINT_JOG_MODE;
						break;
					case 17:
						m_status.printerStatus = PRINT_FATAL_ERROR;
						break;
					}

					// fill in status string
					strPtr = statusCodesToStr(m_status.printerStatus, m_status.printerSubStatus);
					if(strPtr)
						strcpy(m_status.printerStatusStr, strPtr); 
					else 
						m_status.printerStatusStr[0] = '\0';

					break;

				case 'k': // material type, k:xx
					//   xx is material type?
					//   one of 41 46 47 50 51 54 56
					//not used on miniMaker
					//sscanf(buf, "k:%d", &stats.materialType);
					break;

				case 'l': // language, l:ln - one of en, fr, it, de, es, jp
					sscanf(buf, "l:%s", &m_status.lang);
					break;

				case 'm': // ????? m:x,y,z
					sscanf(buf, "m:%d,%d,%d", &d1, &d2, &d3);
					break;

				case 'n': // printer name, n:nm - name as a string
					sscanf(buf, "n:%s", &m_status.machineName);
					break;

				case 'o': // print options, o:ps,tt,cc,al
					//   ps is package size * 1024
					//   tt ???
					//   cc ???
					//   al is auto leveling on if a+
					//o:p8,t1,c1,a+
					sscanf(buf, "o:p%d,t%d,c%d,%s", &d1, &d2, &d3, s1);
					m_status.packetSize = (d1 > 0) ? d1*1024 : 8192;
					m_status.autoLevelEnabled = (0 == strcmp(s1, "a+")) ? true : false;
					break;

				case 'p': // printer model number, p:mn - model_num
					//p:dv1MX0A000
					sscanf(buf, "p:%s", m_status.machineModelNumber);
					m_info = modelToInfo(m_status.machineModelNumber);
					break;

				case 'q': break; // unused
				case 'r': break; // unused

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
					getJsonVal(buf, "buzzer", s1);
					m_status.buzzerEnabled = (0==strcmp(s1, "\"on\"")) ? true : false;
					break;

				case 't': // extruder temperature, t:ss,aa,bb,cc,dd
					//   if ss == 1
					//     aa is extruder temp in C
					//     bb is target temp in C
					//   else
					//     aa is extruder 1 temp
					//     bb is extruder 2 temp
					//     cc is target 1 temp???
					//     dd is target 2 temp???
					//t:1,20,0
					sscanf(buf, "t:%d,%d,%d", &d1, &m_status.extruderActualTemp_C, &m_status.extruderTargetTemp_C);
					break;

				case 'u': break; // unused

				case 'v': // firmware version, v:fw or v:os,ap,fw
					//   fw is firmware version string
					//   os is os version string
					//   ap is app version string
					//v:1.1.1
					sscanf(buf, "v:%s", m_status.firmwareVersion);
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
					sscanf(buf, "w:%d,%s,%s", &d1, m_status.filamentSerialNumber, s1);
					break;

				case 'x': break; // unused
				case 'y': break; // unused

				case 'z': // z offset
					sscanf(buf, "z:%d", &m_status.zOffset);
					break;

				// case 'A' to 'K' unused

				case 'L': // Lifetime timers, L:xx,ml,el,lt
					//   xx - unknown, set to 1
					//   ml - machine lifetime power on time (minutes)
					//   el - extruder lifetime power on time (minutes) (print time)
					//   lt - last power on time (minutes) (or last print time?)
					sscanf(buf, "L:%d,%d,%d,%d", &d1, 
						&m_status.printerLifetimePowerOnTime_min, 
						&m_status.extruderLifetimePowerOnTime_min, 
						&m_status.printerLastPowerOnTime_min);
					break;

				//case 'M' to 'V' unused

				case 'W': // wifi information
					// some sort of json with ssid, bssid, channel, rssiValue, PHY, security
					// not used by miniMaker
					sscanf(buf, "W:%s", s1);
					break;

				case 'X': // Nozzel Info, X:nt,sn
					//   nt is nozzel type one of 
					//     3, 84
					//       nozzle diameter 0.3 mm
					//     1, 2, 77, 82 
					//       nozzle diameter 0.4 mm
					//     54
					//       nozzle diameter 0.6 mm
					//     56 
					//       nozzle diameter 0.8 mm
					//     L, N, H, Q
					//       lazer engraver
					//   sn is serial number in the form xx-xx-xx-xx-xx-yy
					//     where xx is the nozzle serial number
					//     and yy is the total nozzle print time (in minutes)
					sscanf(buf, "X:%d,%s", &m_status.nozzleID, m_status.nozzleSerialNumber);
					m_status.nozzleDiameter_mm = XYZV3::nozzleIDToDiameter(m_status.nozzleID);
					break;

				// case 'Y' to '3' unused

				case '4': // Query IP
					// some sort of json string with wlan, ip, ssid, MAC, rssiValue
					// not used by miniMaker
					sscanf(buf, "4:%s", s1);
					break;

				// case '5' to '9' unused

				case '$': // end of message
					isDone = true;
					m_status.isValid = true;
					break;

				//****FixMe, returns E4$\n or E7$\n sometimes
				// in those cases we just ignore the error and keep going
				case 'E': // error string like E4$\n
					isDone = true;
					m_status.isValid = false;
					break;

				default:
					debugPrint("unknown string: %s\n", buf);
					break;
				}
			}
		}

		success = true;
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::printRawStatus()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		static const int len = 1024;
		char buf[len];
		const char *strPtr = NULL;

		m_serial.writeSerial("XYZv3/query=a");

		// only try so many times for the answer
		bool isDone = false;
		float end = msTime::getTime_s() + 2; // wait a second or two
		while(msTime::getTime_s() < end && !isDone)
		{
			if(m_serial.readSerialLine(buf, len))
			{
				debugPrint(buf);
				if(buf[0] == '$' || buf[0] == 'E')
					isDone = true;
			}
		}

		success = true;
	}

	ReleaseMutex(ghMutex);
	return success;
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

const char* XYZV3::statusCodesToStr(int status, int subStatus)
{
	static char tstr[512];

	switch(status)
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
		return "loading fillament";
	case PRINT_UNLOAD_FILAMENT:
		return "unloading fillament";
	case PRINT_AUTO_CALIBRATION:
		return "auto calibrating";
	case PRINT_JOG_MODE:
		return "jog mode";
	case PRINT_FATAL_ERROR:
		return "fatal error";

	case STATE_PRINT_FILE_CHECK:
		sprintf(tstr, "file check (%d)", subStatus);
		return tstr;
	case STATE_PRINT_LOAD_FIALMENT: //  (substatus 12 14)
		sprintf(tstr, "load fillament (%d)", subStatus);
		return tstr;
	case STATE_PRINT_UNLOAD_FIALMENT: //  (substatus 22 24)
		sprintf(tstr, "unload filament (%d)", subStatus);
		return tstr;
	case STATE_PRINT_JOG_MODE:
		sprintf(tstr, "jog mode (%d)", subStatus);
		return tstr;
	case STATE_PRINT_FATAL_ERROR:
		sprintf(tstr, "fatal error (%d)", subStatus);
		return tstr;
	case STATE_PRINT_HOMING:
		sprintf(tstr, "homing (%d)", subStatus);
		return tstr;
	case STATE_PRINT_CALIBRATE: //  (substatus 41 42 43 44 45 46 47 48 49)
		sprintf(tstr, "calibrating (%d)", subStatus);
		return tstr;
	case STATE_PRINT_CLEAN_NOZZLE: //  (substatus 52)
		sprintf(tstr, "clean nozzle (%d)", subStatus);
		return tstr;
	case STATE_PRINT_GET_SD_FILE:
		sprintf(tstr, "get sd file (%d)", subStatus);
		return tstr;
	case STATE_PRINT_PRINT_SD_FILE:
		sprintf(tstr, "print sd file (%d)", subStatus);
		return tstr;
	case STATE_PRINT_ENGRAVE_PLACE_OBJECT: //  (substatus 30)
		sprintf(tstr, "engrave place object (%d)", subStatus);
		return tstr;
	case STATE_PRINT_ADJUST_ZOFFSET:
		sprintf(tstr, "adjust zoffset (%d)", subStatus);
		return tstr;
	case PRINT_TASK_PAUSED:
		sprintf(tstr, "print paused (%d)", subStatus);
		return tstr;
	case PRINT_TASK_CANCELING:
		sprintf(tstr, "print canceled (%d)", subStatus);
		return tstr;
	case STATE_PRINT_BUSY:
		sprintf(tstr, "busy (%d)", subStatus);
		return tstr;
	case STATE_PRINT_SCANNER_IDLE:
		sprintf(tstr, "scanner idle (%d)", subStatus);
		return tstr;
	case STATE_PRINT_SCANNER_RUNNING:
		sprintf(tstr, "scanner running (%d)", subStatus);
		return tstr;

	default:
		sprintf(tstr, "unknown error (%d:%d)", status, subStatus);
		return tstr;
	}
}

// call to start calibration
bool XYZV3::calibrateBedStart()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerial("XYZv3/action=calibratejr:new");
		if(waitForJsonVal("stat", "start", true) &&
		   waitForJsonVal("stat", "pressdetector", true, 120))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

// ask user to lower detector, then call this
bool XYZV3::calibrateBedRun()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerial("XYZv3/action=calibratejr:detectorok");
		if( waitForJsonVal("stat", "processing", true) &&
			waitForJsonVal("stat", "ok", true, 240))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

// ask user to raise detector, then call this
bool XYZV3::calibrateBedFinish()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		// back out of calibration
		m_serial.writeSerial("XYZv3/action=calibratejr:release");
		waitForJsonVal("stat", "complete", true);
		
		success = true;
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::cleanNozzleStart()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerial("XYZv3/action=cleannozzle:new");
		if( waitForJsonVal("stat", "start", true) &&
			waitForJsonVal("stat", "complete", true, 120)) // wait for nozzle to heat up //****FixMe, show temp...
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::cleanNozzleFinish()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		// always clear out state
		m_serial.writeSerial("XYZv3/action=cleannozzle:cancel");
		if(waitForJsonVal("stat", "ok", true))
			success = true;
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::homePrinter()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;
	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerial("XYZv3/action=home");
		if( waitForJsonVal("stat", "start", true) &&
			waitForJsonVal("stat", "complete", true, 120))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::jogPrinter(char axis, int dist_mm)
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;
	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/action=jog:{\"axis\":\"%c\",\"dir\":\"%c\",\"len\":\"%d\"}",
			axis, (dist_mm < 0) ? '-' : '+', abs(dist_mm));
		if( waitForJsonVal("stat", "start", true) &&
			waitForJsonVal("stat", "complete", true, 120))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::unloadFillament()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerial("XYZv3/action=unload:new");
		if( waitForJsonVal("stat", "start", true) &&
			// waitForJsonVal("stat", "heat", true, 120) && // extemp:22
			waitForJsonVal("stat", "unload", true, 240) &&
			waitForJsonVal("stat", "complete", true, 240))
		{
			success = true;
		}

		// clear out state
		//m_serial.writeSerial("XYZv3/action=unload:cancel");
		//waitForJsonVal("stat", "complete", true);
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::loadFillamentStart()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerial("XYZv3/action=load:new");
		if( waitForJsonVal("stat", "start", true) &&
			//waitForJsonVal("stat", "heat", true, 120) && // extemp:26
			waitForJsonVal("stat", "load", true, 240)) // wait for nozzle to heat up //****FixMe, show temp...
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::loadFillamentFinish()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		// always clear out state
		m_serial.writeSerial("XYZv3/action=load:cancel");
		if(waitForJsonVal("stat", "complete", true))
			success = true;
	}

	ReleaseMutex(ghMutex);
	return success;
}

int XYZV3::incrementZOffset(bool up)
{
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = -1;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/action=zoffset:%s", (up) ? "up" : "down");
		const char* buf = waitForLine(true);
		if(*buf)
		{
			ret = atoi(buf);
		}
	}

	ReleaseMutex(ghMutex);
	return ret;
}

int XYZV3::getZOffset()
{
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = -1;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/config=zoffset:get");
		const char* buf = waitForLine(true);
		if(*buf)
		{
			ret = atoi(buf);
		}
	}

	ReleaseMutex(ghMutex);
	return ret;
}

bool XYZV3::setZOffset(int offset)
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen() && offset > 0)
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/config=zoffset:set[%d]", offset);
		if(waitForVal("ok", true))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::restoreDefaults()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/config=restoredefault:on");
		if(waitForVal("ok", true))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::enableBuzzer(bool enable)
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/config=buzzer:%s", (enable) ? "on" : "off");
		if(waitForVal("ok", true))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::enableAutoLevel(bool enable)
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/config=autolevel:%s", (enable) ? "on" : "off");
		if(waitForVal("ok", true))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::setLanguage(const char *lang)
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen() && lang)
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/config=lang:[%s]", lang);
		if(waitForVal("ok", true))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::cancelPrint()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/config=print[cancel]");
		if(waitForVal("ok", true))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::pausePrint()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/config=print[pause]");
		if(waitForVal("ok", true))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::resumePrint()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/config=print[resume]");
		if(waitForVal("ok", true))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

// call when print finished to prep for a new job
// switches from print done to print ready
// be sure old job is off print bed!!!
bool XYZV3::readyPrint()
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/config=print[complete]");
		if(waitForVal("ok", true))
		{
			success = true;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

/*
//****FixMe, implement these
XYZv3/config=disconnectap
XYZv3/config=engrave:placeobject
XYZv3/config=energy:[E] 
XYZv3/config=life:[XX] 
XYZv3/config=name:[N] 
XYZv3/config=ssid:[WIFIName,Password,AP_Channel] 
XYZv3/config=pda:[1591]
XYZv3/config=pdb:[4387]
XYZv3/config=pdc:[7264]
XYZv3/config=pde:[8046]
*/

bool XYZV3::printFile(const char *path, XYZCallback cbStatus)
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;

	if(path && m_serial.isOpen())
	{
		//****FixMe, temp file should be placed in temp folder
		// see GetTempFileName() and GetTempPath()
		const char *temp = "temp.3w";
		const char *tPath = NULL;

		// if gcode convert to 3w file
		bool isGcode = isGcodeFile(path);
		if(isGcode)
		{
			if(encryptFile(path, temp))
			{
				tPath = temp;
			}
		}
		else if(is3wFile(path))
			tPath = path;
		// else tPath is null and we fail

		if(tPath)
		{
			// send to printer
			if(print3WFile(tPath, cbStatus))
				success = true;

			// cleanup temp file
			if(isGcode)
				remove(temp);
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::print3WFile(const char *path, XYZCallback cbStatus)
{
	bool success = false;
	if(path)
	{
		FILE *f = fopen(path, "rb");
		if(f)
		{
			// get file length
			fseek(f, 0, SEEK_END);
			int len = ftell(f);
			fseek(f, 0, SEEK_SET);

			char *buf = new char[len];
			if(buf)
			{
				if(len == fread(buf, 1, len, f))
				{
					success = print3WString(buf, len, cbStatus);
				}

				delete [] buf;
				buf = NULL;
			}

			fclose(f);
		}
	}

	return success;
}

#if 0
// print directly to serial port
bool XYZV3::print3WString(const char *data, int len, XYZCallback cbStatus)
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool saveToSD = false; // set to true to save to internal SD card
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/upload=temp.gcode,%d%s\n", len, (saveToSD) ? ",SaveToSD" : "");
		if(waitForVal("ok", false))
		{
			int blockSize = (m_status.isValid) ? m_status.packetSize : 8192;
			int blockCount = (len + blockSize - 1) / blockSize; // round up
			int lastBlockSize = len % blockSize;

			for(int i=0; i<blockCount; i++)
			{
				// returns E4 and E7, whatever that is, but ignores the error and continues
				int blockLen = (i+1 == blockCount) ? lastBlockSize : blockSize;
				m_serial.writeSerialByteU32(i);
				m_serial.writeSerialByteU32(blockLen);
				m_serial.writeSerialArray(data + i*blockSize, blockLen);
				m_serial.writeSerialByteU32(0);

				if(waitForVal("ok", false))
					success = true;
				else
				{
					// bail on error
					success = false;
					break;
				}

				// give time to parrent
				if(cbStatus)
					cbStatus((float)i/(float)blockCount);
			}
		}

		// close out printing
		m_serial.writeSerial("XYZv3/uploadDidFinish");
		//waitForVal("ok", true);
	}

	ReleaseMutex(ghMutex);
	return success;
}
#else
// queue up in buffer before printing
// may help reduce chance of E7 errors
bool XYZV3::print3WString(const char *data, int len, XYZCallback cbStatus)
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool saveToSD = false; // set to true to save to internal SD card
	bool success = false;

	if(m_serial.isOpen())
	{
		m_serial.clearSerial();

		m_serial.writeSerialPrintf("XYZv3/upload=temp.gcode,%d%s\n", len, (saveToSD) ? ",SaveToSD" : "");
		if(waitForVal("ok", false))
		{
			int blockSize = (m_status.isValid) ? m_status.packetSize : 8192;
			int blockCount = (len + blockSize - 1) / blockSize; // round up
			int lastBlockSize = len % blockSize;
			int t;

			char *bBuf = new char[blockSize + 12];
			if(bBuf)
			{
				for(int i=0; i<blockCount; i++)
				{
					int blockLen = (i+1 == blockCount) ? lastBlockSize : blockSize;
					char *tBuf = bBuf;

					// block count
					t = swap32bit(i);
					memcpy(tBuf, &t, 4);
					tBuf += 4;

					// data length
					t = swap32bit(blockLen);
					memcpy(tBuf, &t, 4);
					tBuf += 4;

					// data block
					memcpy(tBuf, data + i*blockSize, blockLen);
					tBuf += blockLen;

					// end of data
					t = swap32bit(0);
					memcpy(tBuf, &t, 4);
					tBuf += 4;

					// write out in one shot
					m_serial.writeSerialArray(bBuf, blockLen + 12);
					success = waitForVal("ok", false);
					if(!success) // bail on error
						break;

					// give time to parrent
					if(cbStatus)
						cbStatus((float)i/(float)blockCount);
				}
				delete [] bBuf;
			}
		}

		// close out printing
		m_serial.writeSerial("XYZv3/uploadDidFinish");
		if(!waitForVal("ok", false))
			success = false;
	}

	ReleaseMutex(ghMutex);
	return success;
}
#endif

bool XYZV3::writeFirmware(const char *path, XYZCallback cbStatus)
{
	return false; // probably don't want to run this!

	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;
	bool downgrade = false;

	if(path && m_serial.isOpen())
	{
		m_serial.clearSerial();

		FILE *f = fopen(path, "rb");
		if(f)
		{
			// get file length
			fseek(f, 0, SEEK_END);
			int len = ftell(f);
			fseek(f, 0, SEEK_SET);

			char *buf = new char[len];
			if(buf)
			{
				if(len == fread(buf, 1, len, f))
				{
					// first 16 bytes are header
					// don't send that
					int blen = len - 16; 
					char *bbuf = buf + 16;

					m_serial.writeSerialPrintf("XYZv3/firmware=temp.bin,%d%s\n", blen, (downgrade) ? ",Downgrade" : "");
					if(waitForVal("ok", false))
					{
						int blockSize = (m_status.isValid) ? m_status.packetSize : 8192;
						int blockCount = (blen + blockSize - 1) / blockSize; // round up
						int lastBlockSize = blen % blockSize;

						for(int i=0; i<blockCount; i++)
						{
							int blockLen = (i+1 == blockCount) ? lastBlockSize : blockSize;
							m_serial.writeSerialByteU32(i);
							m_serial.writeSerialByteU32(blockLen);
							m_serial.writeSerialArray(bbuf + i*blockSize, blockLen);
							m_serial.writeSerialByteU32(0);

							if(waitForVal("ok", false))
								success = true;
							else
							{
								// bail on error
								success = false;
								break;
							}

							// give time to parrent
							if(cbStatus)
								cbStatus((float)i/(float)blockCount);
						}
					}

					// close out printing
					m_serial.writeSerial("XYZv3/uploadDidFinish");
					//waitForVal("ok", true);
				}

				delete [] buf;
				buf = NULL;
			}

			fclose(f);
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::convertFile(const char *inPath, const char *outPath)
{
	if(m_info && inPath)
	{
		if(isGcodeFile(inPath))
			return encryptFile(inPath, outPath);
		else if(is3wFile(inPath))
			return decryptFile(inPath, outPath);
	}

	return false;
}

bool XYZV3::isGcodeFile(const char *path)
{
	if(path)
	{
		const char *p = strrchr(path, '.');
		if(p)
			return 0 == strcmp(p, ".gcode") ||
				   0 == strcmp(p, ".gco") ||
				   0 == strcmp(p, ".g");
	}

	return false;
}

bool XYZV3::is3wFile(const char *path)
{
	if(path)
	{
		const char *p = strrchr(path, '.');
		if(p)
			return 0 == strcmp(p, ".3w");
	}

	return false;
}

bool XYZV3::encryptFile(const char *inPath, const char *outPath)
{
	WaitForSingleObject(ghMutex, INFINITE);
	bool success = false;
	const int bodyOffset = 8192;

	// for now encrypt the file for the connected printer
	// someday we could allow user to specify the target machine
	if(m_info && inPath && m_serial.isOpen())
	{
		const char *fileNum = m_info->fileNum;
		bool fileIsV5 = m_info->fileIsV5;
		bool fileIsZip = m_info->fileIsZip;

		// open our source file
		FILE *fi = fopen(inPath, "rb");
		if(fi)
		{
			// and write to disk
			FILE *f = NULL;
			if(outPath)
				f = fopen(outPath, "wb");
			else
			{
				char tPath[MAX_PATH] = "";
				strcpy(tPath, inPath);
				char *ptr = strrchr(tPath, '.');
				if(!ptr)
					ptr = tPath + strlen(tPath);
				sprintf(ptr, ".3w");
				f = fopen(tPath, "wb");
			}

			// and our destination file
			if(f)
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
					int headerLen = 0, bodyLen = 0;
					char *bodyBuf = NULL, *headerBuf = NULL;
					if(processGCode(gcode, gcodeLen, fileNum, fileIsV5, fileIsZip, &headerBuf, &headerLen, &bodyBuf, &bodyLen))
					{
						//==================
						// write header info

						// write file id
						fwrite("3DPFNKG13WTW", 1, strlen("3DPFNKG13WTW"), f);

						// id, what is this
						writeByte(f, 1);

						// file format version is 2 or 5
						if(fileIsV5)
							writeByte(f, 5);
						else
							writeByte(f, 2);
						
						// pad to 4 bytes
						writeByte(f, 0);
						writeByte(f, 0);

						// offset to zip marker
						int pos1 = ftell(f) + 4; // count from next byte after count
						// force at least 16 bytes of padding, is this needed?
						int zipOffset = roundUpTo16(pos1 + 4684) - pos1;
						writeWord(f, zipOffset);
						writeRepeatByte(f, 0, zipOffset);

						// zip format marker
						if(fileIsZip)
							fwrite("TagEa128", 1, strlen("TagEa128"), f);
						else
							fwrite("TagEJ256", 1, strlen("TagEJ256"), f);

						// optional header len
						if(fileIsV5)
							writeWord(f, headerLen);

						// offset to header
						int pos2 = ftell(f) + 4; // count from next byte after count
						// force at least 16 bytes of padding, is this needed?
						int headerOffset = roundUpTo16(pos2 + 68) - pos2; 
						writeWord(f, headerOffset);

						// mark current file location
						int offset1 = ftell(f);

						//?? 
						if(fileIsV5)
							writeWord(f, 1);

						int crc32 = calcXYZcrc32(bodyBuf, bodyLen);
						writeWord(f, crc32);

						// zero pad to header offset
						int pad1 = headerOffset - (ftell(f) - offset1);
						writeRepeatByte(f, 0, pad1);

						// write encrypted and padded header out
						fwrite(headerBuf, 1, headerLen, f);

						// mark current file location
						int pad2 = bodyOffset - ftell(f);
						// pad with zeros to start of body
						writeRepeatByte(f, 0, pad2);

						// write encrypted and padded body out
						fwrite(bodyBuf, 1, bodyLen, f);

						// yeay, it worked
						success = true;

						// cleanup
						delete [] headerBuf;
						headerBuf = NULL;

						delete [] bodyBuf;
						bodyBuf = NULL;
					}

					delete [] gcode;
					gcode = NULL;
				}

				// clean up file data
				fclose(f);
				f = NULL;
			}

			fclose(fi);
			fi = NULL;
		}
	}

	ReleaseMutex(ghMutex);
	return success;
}

bool XYZV3::decryptFile(const char *inPath, const char *outPath)
{
	bool success = false;

	if(inPath)
	{
		FILE *f = fopen(inPath, "rb");
		if(f)
		{
			const int bodyOffset = 8192;
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
				int id, fv;
				id = buf[0];
				fv = buf[1];

				// offset to zip marker
				int zipOffset = readWord(f);
				fseek(f, zipOffset, SEEK_CUR);

				// zip format marker
				fread(buf, 1, 8, f);
				bool isZip = false;
				if(0 == strncmp("TagEa128", buf, strlen("TagEa128")))
					isZip = true;
				else if(0 == strncmp("TagEJ256", buf, strlen("TagEJ256")))
					isZip = false;
				//else error

				// optional header len
				int headerLen = (fv == 5) ? readWord(f) : -1;

				// offset to header
				int headerOffset = readWord(f);

				int offset1 = ftell(f);

				//?? 
				int id2 = (fv == 5) ? readWord(f) : -1;

				int crc32 = readWord(f);

				int offset = headerOffset - (ftell(f) - offset1);
				fseek(f, offset, SEEK_CUR);

				// header potentially goes from here to 8192
				// v5 format stores header len, but v2 files you just have
				// to search for zeros to indicate the end...
				//****Note, header is duplicated in body, for now just skip it
				// you can uncomment this if you want to verify this is the case
#ifdef PARCE_HEADER
				if(headerLen < 1)
					headerLen = bodyOffset - ftell(f);
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
					pkcs7unpad(hbuf, headerLen);

					//****FixMe, do something with it
					debugPrint(hbuf);

					delete [] hbuf;
					hbuf = NULL;
				}
#endif

				// body contains duplicate header and body of gcode file
				// and always starts at 8192 (0x2000)

				fseek(f, bodyOffset, SEEK_SET);
				int bodyLen = totalLen - ftell(f);
				char *bbuf = new char[bodyLen + 1];
				if(bbuf)
				{
					fread(bbuf, 1, bodyLen, f);
					bbuf[bodyLen] = '\0';

					if(crc32 != calcXYZcrc32(bbuf, bodyLen))
						debugPrint("crc's don't match!!!\n");

					if(isZip)
					{
						//****FixMe
						// Im not really sure on the order here, need to find a file to look at

						// decrypt body
					    struct AES_ctx ctx;
						uint8_t iv[16] = {0}; // 16 zeros
						char *key = "@xyzprinting.com";
					    AES_init_ctx_iv(&ctx, key, iv);
					    AES_CBC_decrypt_buffer(&ctx, (uint8_t*)bbuf, bodyLen);

						//****FixMe, file was padded out to 8192, remove padding somehow

						//****FixMe, unzip file
					}
					else
					{
						// first char in an unencrypted file will be ';'
						// so check if no encrypted, v5 files sometimes are not encrypted
						if(bbuf[0] != ';')
						{
							// decrypt body
						    struct AES_ctx ctx;
							uint8_t iv[16] = {0}; // 16 zeros
							char *key = "@xyzprinting.com@xyzprinting.com";
						    AES_init_ctx_iv(&ctx, key, iv);
						    AES_ECB_decrypt_buffer(&ctx, (uint8_t*)bbuf, bodyLen);
						}

						// remove any padding from body
						pkcs7unpad(bbuf, bodyLen);
					}

					// and write to disk
					FILE *fo = NULL;
					if(outPath)
						fo = fopen(outPath, "wb");
					else
					{
						char tPath[MAX_PATH];
						strcpy(tPath, inPath);
						char *ptr = strrchr(tPath, '.');
						if(!ptr)
							ptr = tPath + strlen(tPath);
						sprintf(ptr, ".gcode");
						fo = fopen(tPath, "wb");
					}

					if(fo)
					{
						fprintf(fo, bbuf);
						fclose(fo);
						fo = NULL;
						success = true;
					}

					delete [] bbuf;
					bbuf = NULL;
				}
			}

			fclose(f);
		}
	}

	return success;
}


//------------------------------------------

const char* XYZV3::waitForLine(bool waitForEndCom, int timeout_s)
{
	static const int len = 1024;
	static char buf[len]; //****Note, this buffer is overwriten every time you call waitForLine!!!
	*buf = '\0';

	if(m_serial.isOpen())
	{
		// only try so many times for the answer
		float end = msTime::getTime_s() + timeout_s;
		while(msTime::getTime_s() < end)
		{
			if(m_serial.readSerialLine(buf, len))
			{
				if(buf[0] == '$')
					return "";
				//if(buf[0] == 'E') //****FixMe, test out error detection to speed up communication
				//	return "";

				break;
			}
			Sleep(1);
		}
		if(msTime::getTime_s() >= end)
			debugPrint("waitForLine, timeout triggered %d\n", timeout_s);

		if(waitForEndCom)
		{
			// check for '$' indicating end of message
			char buf2[len];
			end = msTime::getTime_s() + timeout_s;
			while(msTime::getTime_s() < end)
			{
				if(m_serial.readSerialLine(buf2, len))
				{
					if(buf2[0] == '$')
						return buf;
				}
				Sleep(1);
			}
			if(msTime::getTime_s() >= end)
				debugPrint("waitForLine $ timeout triggered %d\n", timeout_s);
		}
	}

	return buf;
}

bool XYZV3::waitForVal(const char *val, bool waitForEndCom, int timeout_s)
{
	if(val)
	{
		const char* buf = waitForLine(waitForEndCom, timeout_s);
		if(0 == strcmp(val, buf))
		{
			return true;
		}
	}

	return false;
}

bool XYZV3::waitForJsonVal(const char *key, const char*val, bool waitForEndCom, int timeout_s)
{
	if(key && val)
	{
		static const int len = 1024;
		char tVal[len];

		float end = msTime::getTime_s() + timeout_s;
		while(msTime::getTime_s() < end)
		{
			const char* buf = waitForLine(waitForEndCom, timeout_s);
			if(getJsonVal(buf, key, tVal))
			{
				// check for exact match
				if(0 == strcmp(val, tVal))
					return true;

				// check for quoted match, probably not an ideal test
				if(*tVal && 0 == strncmp(val, tVal+1, strlen(val)))
					return true;
			}
		}
	}

	return false;
}

bool XYZV3::getJsonVal(const char *str, const char *key, char *val)
{
	if(str && key && val)
	{
		*val = '\0';

		char fullKey[256];
		sprintf(fullKey, "\"%s\":", key);
		const char *offset = strstr(str, fullKey);

		if(offset)
		{
			offset += strlen(fullKey);
		
			sscanf(offset, "%[^,}]", val);

			if(val && *val)
				return true;
		}
	}

	return false;
}

unsigned int XYZV3::swap16bit(unsigned int in)
{
	return (in >> 8) & 0x00FF | (in << 8) & 0xFF00;
}

unsigned int XYZV3::swap32bit(unsigned int in)
{
	return 
		(in >> 24) & 0x000000FF |
		(in >> 8)  & 0x0000FF00 |
		(in << 8)  & 0x00FF0000 |
		(in << 24) & 0xFF000000 ;
}

// read a word from a file, in big endian format
int XYZV3::readWord(FILE *f)
{
	int i = 0;
	if(f)
		fread(&i, 4, 1, f);
	return swap32bit(i);
}

// write a word to a file, in big endian format
void XYZV3::writeWord(FILE *f, int i)
{
	if(f)
	{
		int t = swap32bit(i);
		fwrite(&t, 4, 1, f);
	}
}

int XYZV3::readByte(FILE *f)
{
	char i = 0;
	if(f)
		fread(&i, 1, 1, f);
	return (int)i;
}

void XYZV3::writeByte(FILE *f, char c)
{
	if(f)
		fwrite(&c, 1, 1, f);
}

void XYZV3::writeRepeatByte(FILE *f, char byte, int count)
{
	if(f)
	{
		for(int i=0; i<count; i++)
			fwrite(&byte, 1, 1, f);
	}
}

int XYZV3::roundUpTo16(int in)
{
	return (in + 15) & 0xFFFFFFF0;
}

void XYZV3::pkcs7unpad(char *buf, int len)
{
	if(buf && len > 0)
	{
		int count = buf[len-1];
		if(count > 0 && count < 16)
			buf[len-count] = '\0';
	}
}

// note, expects buf to have room to be padded out
int XYZV3::pkcs7pad(char *buf, int len)
{
	if(buf && len > 0)
	{
		int newLen = roundUpTo16(len);
		int count = newLen - len;

		if(count > 0 && count < 16)
		{
			for(int i=0; i<count; i++)
				buf[len+i] = count;
		}

		return newLen;
	}

	return len;
}

// XYZ's version of CRC32, does not seem to match the standard algorithms
unsigned int XYZV3::calcXYZcrc32(char *buf, int len)
{
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

	unsigned int num = -1;

	if(buf && len > 0)
	{
		for (int i = 0; i < len; i++)
			num = num >> 8 ^ hashTable[(num ^ buf[i]) & 255];

		num = (unsigned int)((unsigned long long)num ^ (long long)-1);
		if (num < 0)
			num = num;
	}

	return num;
}

const char* XYZV3::readLineFromBuf(const char* buf, char *lineBuf, int lineLen)
{
	// zero out buffer
	if(lineBuf)
		*lineBuf = '\0';

	if(buf && *buf && lineBuf && lineLen > 0)
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

	return NULL;
}

bool XYZV3::checkLineIsHeader(const char* lineBuf)
{
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

	return true; // else just white space, assume header
}

bool XYZV3::processGCode(const char *gcode, const int gcodeLen, const char *fileNum, bool fileIsV5, bool fileIsZip, char **headerBuf, int *headerLen, char **bodyBuf, int *bodyLen)
{
	
	const int lineLen = 1024;
	char lineBuf[lineLen];

	// validate parameters
	if(gcode && gcodeLen > 1 && headerBuf && headerLen && bodyBuf && bodyLen)
	{
		// parse header once to get info on print time
		int printTime = 1;
		float totalFilamen = 1.0f;

		char *s = NULL;
		const char *tcode = gcode;
		tcode = readLineFromBuf(tcode, lineBuf, lineLen);
		while(*lineBuf)
		{
			if(checkLineIsHeader(lineBuf))
			{
				// strip out the following parameters
				// and capture info if possible
				// ; filename = temp.3w
				// ; print_time = {estimated time in seconds}
				// ; machine = dv1MX0A000 // change to match current printer
				// ; total_filament = {estimated filament used in mm}

				// print time
				if(NULL != (s = strstr(lineBuf, "print_time = ")))
					printTime = atoi(s + strlen("print_time = "));
				else if(NULL != (s = strstr(lineBuf, "TIME:")))
					printTime = atoi(s + strlen("TIME:"));

				// fillament used
				else if(NULL != (s = strstr(lineBuf, "total_filament = ")))
					totalFilamen = (float)atof(s + strlen("total_filament = "));
				else if(NULL != (s = strstr(lineBuf, "filament_used = ")))
					totalFilamen = (float)atof(s + strlen("filament_used = "));
				else if(NULL != (s = strstr(lineBuf, "Filament used: ")))
					totalFilamen = 1000.0f * (float)atof(s + strlen("Filament used: ")); // m to mm
			}
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
			bbufOffset += sprintf(bBuf + bbufOffset, "; total_filament = %0.2f\n", totalFilamen);

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
					if( NULL != strstr(lineBuf, "filename") || 
						NULL != strstr(lineBuf, "machine") ||
						NULL != (s = strstr(lineBuf, "print_time")) ||
						NULL != (s = strstr(lineBuf, "total_filament")) )
					{
						// drop the line on the floor
					}
					else // else just pass it on through
						bbufOffset += sprintf(bBuf + bbufOffset, lineBuf);
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
					bbufOffset += sprintf(bBuf + bbufOffset, lineBuf);
				}

				tcode = readLineFromBuf(tcode, lineBuf, lineLen);
			}

			//==============
			// fix up header

			// padded out to 16 byte boundary
			// and copy so we can encrypt it seperately
			int hLen = roundUpTo16(headerEnd); 
			char *hBuf = new char[hLen+1];
			if(hBuf)
			{
				memcpy(hBuf, bBuf, headerEnd);

				// don't forget to tag the padding
				pkcs7pad(hBuf, headerEnd);
				hBuf[hLen] = '\0';

				// encrypt the header in CBC mode
				// it appears that v5 files don't always encrypt
				if(!fileIsV5)
				{
				    struct AES_ctx ctx;
					uint8_t iv[16] = {0}; // 16 zeros

					char *hkey = "@xyzprinting.com";
				    AES_init_ctx_iv(&ctx, hkey, iv);
				    AES_CBC_encrypt_buffer(&ctx, (uint8_t*)hBuf, hLen);
				}

				//============
				// fix up body

				if(fileIsZip)
				{
					// not finished, can't count on this code
					assert(false);

					int bLen = bbufOffset;

					// Im not really sure on the order here, need to find a file to look at

					//****FixMe zip file

					//****FixMe, pad out to 8192

					// encrypt body
				    struct AES_ctx ctx;
					uint8_t iv[16] = {0}; // 16 zeros
					char *key = "@xyzprinting.com";
				    AES_init_ctx_iv(&ctx, key, iv);
				    AES_CBC_encrypt_buffer(&ctx, (uint8_t*)bBuf, bLen);

					//****FixMe, set body output
					*bodyLen = bLen;
				}
				else
				{
					int bLen = pkcs7pad(bBuf, bbufOffset);

					// encrypt body using ECB mode
				    struct AES_ctx ctx;
					uint8_t iv[16] = {0}; // 16 zeros
					char *bkey = "@xyzprinting.com@xyzprinting.com";
				    AES_init_ctx_iv(&ctx, bkey, iv);
				    AES_ECB_encrypt_buffer(&ctx, (uint8_t*)bBuf, bLen);

					// bBuf already padded out, no need to copy it over
					// just mark the padding
					*bodyLen = bLen;
				}

				*headerLen = hLen;
				*headerBuf = hBuf;
				*bodyBuf = bBuf;

				return true;
			}

			delete [] bBuf;
		}

		// make sure we return something
		*headerBuf = NULL;
		*headerLen = 0;
		*bodyBuf = NULL;
		*bodyLen = 0;
	}

	return false;
}

const XYZPrinterInfo* XYZV3::modelToInfo(const char *modelNum)
{
	if(modelNum)
	{
		for(int i=0; i<m_infoArrayLen; i++)
		{
			if(0 == strcmp(modelNum, m_infoArray[i].modelNum))
			{
				return &m_infoArray[i];
			}
		}
	}

	return NULL;
}

#define ERR_C_BUFFER_SIZE 2048
void XYZV3::debugPrint(char *format, ...)
{
	char msgBuf[ERR_C_BUFFER_SIZE];
	va_list arglist;

	va_start(arglist, format);
	_vsnprintf(msgBuf, sizeof(msgBuf), format, arglist);
	msgBuf[sizeof(msgBuf)-1] = '\0';
	va_end(arglist);

#ifdef _CONSOLE
	printf("%s\n", msgBuf);
#else
	OutputDebugString(msgBuf);
#endif
}
