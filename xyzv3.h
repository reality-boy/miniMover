#ifndef XYZV3_H
#define XYZV3_H

// forward declerations

class Serial; // from serial.h

// printer status word
// items marked with state have substatus words as well.
enum XYZPrintStatusCode
{
	PRINT_INITIAL = 9500,
	PRINT_HEATING = 9501,
	PRINT_PRINTING = 9502,
	PRINT_CALIBRATING = 9503,
	PRINT_CALIBRATING_DONE = 9504,
	PRINT_IN_PROGRESS = 9505,
	PRINT_COOLING_DONE = 9506,
	PRINT_COOLING_END = 9507,
	PRINT_ENDING_PROCESS = 9508,
	PRINT_ENDING_PROCESS_DONE = 9509,
	PRINT_JOB_DONE = 9510,
	PRINT_NONE = 9511,
	PRINT_STOP = 9512,
	PRINT_LOAD_FILAMENT = 9513,
	PRINT_UNLOAD_FILAMENT = 9514,
	PRINT_AUTO_CALIBRATION = 9515,
	PRINT_JOG_MODE = 9516,
	PRINT_FATAL_ERROR = 9517,

	STATE_PRINT_FILE_CHECK = 9520,

	STATE_PRINT_LOAD_FIALMENT = 9530,
	STATE_PRINT_UNLOAD_FIALMENT = 9531,
	STATE_PRINT_JOG_MODE = 9532,
	STATE_PRINT_FATAL_ERROR = 9533,
	STATE_PRINT_HOMING = 9534,
	STATE_PRINT_CALIBRATE = 9535,
	STATE_PRINT_CLEAN_NOZZLE = 9536,
	STATE_PRINT_GET_SD_FILE = 9537,
	STATE_PRINT_PRINT_SD_FILE = 9538,
	STATE_PRINT_ENGRAVE_PLACE_OBJECT = 9539,
	STATE_PRINT_ADJUST_ZOFFSET = 9540,

	PRINT_TASK_PAUSED = 9601,
	PRINT_TASK_CANCELING = 9602,

	STATE_PRINT_BUSY = 9700,

	STATE_PRINT_SCANNER_IDLE = 9800,
	STATE_PRINT_SCANNER_RUNNING = 9801,
};

// status of current printer
struct XYZPrinterState
{
	bool isValid;

	int bBedActualTemp_C;

	bool cCalibIsValid;
	int cCalib[9];

	int dPrintPercentComplete;
	int dPrintElapsedTime_m;
	int dPrintTimeLeft_m;

	char eErrorStatusStr[64];
	int eErrorStatus;

	int fFillimantSpoolCount;
	int fFillimant1Remaining_mm;
	int fFillimant2Remaining_mm;

	int hIsFillamentPLA;

	char iMachineSerialNum[64]; //20

	XYZPrintStatusCode jPrinterStatus;
	int jPrinterSubStatus;
	char jPrinterStatusStr[256];

	int kMaterialType;

	char lLang[32]; //4

	int mVal[3]; // unknown m:x,y,z values

	char nMachineName[64]; //32

	int oPacketSize;
	int oT; // unknown from o: command
	int oC; // unknown from o: command
	bool oAutoLevelEnabled;

	char pMachineModelNumber[64]; //12

	bool sBuzzerEnabled;
	bool sButton;
	bool sFrontDoor;
	bool sTopDoor;
	bool sHasLazer;
	bool sFd;
	bool sFm;
	bool sOpenFilament;
	bool sSDCard;

	int tExtruderCount;
	int tExtruder1ActualTemp_C;
	int tExtruder2ActualTemp_C;
	int tExtruderTargetTemp_C; // set by t: or O:

	char vFirmwareVersion[64]; //10

	int wFilamentCount;
	char wFilament1SerialNumber[64]; //16
	char wFilament2SerialNumber[64]; //16

	int zOffset; // from getZOffset()

	char GLastUsed[64];

	int LExtruderCount;
	int LPrinterLifetimePowerOnTime_min;
	int LPrinterLastPowerOnTime_min;
	int LExtruderLifetimePowerOnTime_min;
	// I suspect there are two of these on a dual color pinter

	int OBedTargetTemp_C;

	char VString[64]; // unknown V:version number

	char WSSID[64];
	char WBSSID[64];
	char WChannel[64];
	char WRssiValue[64];
	char WPHY[64];
	char WSecurity[64];

	int XNozzleID;
	float XNozzleDiameter_mm;
	char XNozzle1SerialNumber[64]; //32
	char XNozzle2SerialNumber[64]; //32

	char N4NetIP[64]; //16
	char N4NetSSID[64];
	char N4NetChan[64];
	char N4NetMAC[64];
	char N4NetRssiValue[64];
};

// info on each printer type
struct XYZPrinterInfo
{
	// string constants
	const char *modelNum;
	const char *fileNum;
	const char *serialNum;

	bool fileIsV5; // v2 is 'normal' file format, v5 is for 'pro' systems
	bool fileIsZip; // older file format, zips header
	bool comIsV3; // old or new serial protocol, v3 is new

	// build volume
	int length;
	int width;
	int height;

	const char *screenName;
};

typedef void (*XYZCallback)(float pct);

// master class
// most functions will block while serial IO happens
class XYZV3
{
public:
	XYZV3();
	~XYZV3(); 

	//===========================
	// serial communication

	// port is com port number, -1 is auto detect
	bool connect(int port = -1);
	void disconnect();
	bool isConnected(); // return true if connected

	// expose serial functionality
	static int refreshPortList();
	static int getPortCount();
	static int getPortNumber(int id);
	static const char* getPortName(int id);

	//****FixMe, statusCode and status are too close together!!!!
	bool updateStatus(); // update status struct
	const XYZPrinterState* getPrinterState() { return &m_status; }
	const XYZPrinterInfo* getPrinterInfo() { return m_info; }

	// dump what is returned from the serial port to console
	void printAllLines();
	void printRawStatus(); 

	// run auto bed leveling routine
	// call to start
	bool calibrateBedStart(); 
	// ask to lower detector then call
	bool calibrateBedRun();
	// ask to raise detector then call
	bool calibrateBedFinish();

	// heats up nozzle so you can clean it out with a wire
	bool cleanNozzleStart();
	bool cleanNozzleFinish(); // call once user finishes cleaning

	// home printer so you can safely move head
	bool homePrinter();

	// move head in given direction, axis is one of x,y,z
	bool jogPrinter(char axis, int dist_mm);

	// unloads fillament without any user interaction
	bool unloadFillament();

	// loads fillament
	bool loadFillamentStart();
	// call when fillament comes out of extruder!!!
	bool loadFillamentFinish();

	// increment/decrement z offset by one step (1/100th of  a mm?)
	int incrementZOffset(bool up);
	int getZOffset(); // in hundredths of a mm
	bool setZOffset(int offset);

	bool restoreDefaults();
	bool enableBuzzer(bool enable);
	bool enableAutoLevel(bool enable);
	bool setLanguage(const char *lang); //lang is one of en, fr, it, de, es, jp

	bool cancelPrint();
	bool pausePrint();
	bool resumePrint();
	bool readyPrint();

	// print a gcode or 3w file, convert as needed
	bool printFile(const char *path, XYZCallback cbStatus);

	// load new firmware into the printer.
	// probably not a good idea to mess with this!
	bool writeFirmware(const char *path, XYZCallback cbStatus);

	//=====================
	// file i/o

	// convert a gcode file to 3w format
	// infoIdx is the index into the m_infoArray or -1 for auto
	bool encryptFile(const char *inPath, const char *outPath = NULL, int infoIdx = -1);

	// convert a 3w file to gcode
	bool decryptFile(const char *inPath, const char *outPath = NULL);

	// convert from gcode to 3w or back depending on file type
	// infoIdx is the index into the m_infoArray or -1 for auto
	bool convertFile(const char *inPath, const char *outPath = NULL, int infoIdx = -1);
	bool isGcodeFile(const char *path);
	bool is3wFile(const char *path);

	static int getInfoCount();
	static const XYZPrinterInfo* indexToInfo(int index);
	static const XYZPrinterInfo* modelToInfo(const char *modelNum);

protected:

	// send a .3w file to printer
	bool print3WFile(const char *path, XYZCallback cbStatus);
	bool print3WString(const char *data, int len, XYZCallback cbStatus);

	const char* statusCodesToStr(int status, int subStatus);
	static float nozzleIDToDiameter(int id);
	const char* errorCodeToStr(int code);
	bool getJsonVal(const char *str, const char *key, char *val);

	// serial functions
	const char* waitForLine(bool waitForEndCom, float timeout_s = 0.5f);
	bool waitForVal(const char *val, bool waitForEndCom, float timeout_s = 0.5f);
	bool waitForJsonVal(const char *key, const char *val, bool waitForEndCom, float timeout_s = 0.5f);

	// file functions
	unsigned int swap16bit(unsigned int in);
	unsigned int swap32bit(unsigned int in);

	int readWord(FILE *f);
	void writeWord(FILE *f, int i);
	int readByte(FILE *f);
	void writeByte(FILE *f, char c);
	void writeRepeatByte(FILE *f, char byte, int count);

	int roundUpTo16(int in);
	int pkcs7unpad(char *buf, int len);
	int pkcs7pad(char *buf, int len);

	unsigned int calcXYZcrc32(char *buf, int len);
	const char* readLineFromBuf(const char* buf, char *lineBuf, int lineLen);
	bool checkLineIsHeader(const char* lineBuf);
	bool processGCode(const char *gcode, const int gcodeLen, const char *fileNum, bool fileIsV5, bool fileIsZip, char **headerBuf, int *headerLen, char **bodyBuf, int *bodyLen);

	Serial m_serial;
	XYZPrinterState m_status;
	const XYZPrinterInfo *m_info;

	static const int m_infoArrayLen = 19;
	static const XYZPrinterInfo m_infoArray[m_infoArrayLen];

	HANDLE ghMutex;
};

#endif // XYZV3_H