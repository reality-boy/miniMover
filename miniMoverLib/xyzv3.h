#ifndef XYZV3_H
#define XYZV3_H

/*
Class that allows you to control any XYZ da Vinci printer 
that uses the version 3 communication protocol.  This includes
the jr mini and micro lines and most of the new pro printers as
well, ecept for possibly the color.

This fully replicates the XYZMaker software allowing you to
set all features on the printer, allong with converting from
a gcode file to a 3w file and back. The only thing it can't do 
that XYZMaker does do is slice a model for you.  You will need
to use a third party slicing tool to generate the gcode.
*/

extern const char* g_ver;

// printer state word
// items marked with state have substatus words as well.
enum XYZPrintStateCode
{
	PRINT_INITIAL = 9500,
	PRINT_HEATING = 9501,
	PRINT_PRINTING = 9502,
	PRINT_CALIBRATING = 9503,
	PRINT_CALIBRATING_DONE = 9504,
	PRINT_IN_PROGRESS = 9505,		// print 2:
	PRINT_COOLING_DONE = 9506,
	PRINT_COOLING_END = 9507,
	PRINT_ENDING_PROCESS = 9508,
	PRINT_ENDING_PROCESS_DONE = 9509,	// print 3:
	PRINT_JOB_DONE = 9510,
	PRINT_NONE = 9511,
	PRINT_STOP = 9512,
	PRINT_LOAD_FILAMENT = 9513,
	PRINT_UNLOAD_FILAMENT = 9514,
	PRINT_AUTO_CALIBRATION = 9515,
	PRINT_JOG_MODE = 9516,
	PRINT_FATAL_ERROR = 9517,

	STATE_PRINT_FILE_CHECK = 9520,	// print 1: pre-printing, file compatibility check?

	STATE_PRINT_LOAD_FIALMENT = 9530,
		// sub 11 - warming up
		// sub 12 - loading

	STATE_PRINT_UNLOAD_FIALMENT = 9531,
		// sub 21 - warming up
		// sub 22 - unloading

	STATE_PRINT_JOG_MODE = 9532,			
		// sub 30 - jogging

	STATE_PRINT_FATAL_ERROR = 9533,

	STATE_PRINT_HOMING = 9534,				
		// sub 5 - homing

	STATE_PRINT_CALIBRATE = 9535,			
		// sub 40 - begin calibration
		// sub 41 - ask to lower detector
		// sub 43 - calibrating
		// sub 44 - ask to raise detector

	STATE_PRINT_CLEAN_NOZZLE = 9536,
		// sub 51 - warming up
		// sub 52 - ready to clean

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
struct XYZPrinterStatus
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
	int eErrorStatusHiByte;

	int fFilamentSpoolCount;
	int fFilament1Remaining_mm;
	int fFilament2Remaining_mm;

	int hIsFilamentPLA;

	char iMachineSerialNum[64]; //20

	XYZPrintStateCode jPrinterState;
	int jPrinterSubState;
	char jPrinterStateStr[256];

	int kFilamentMaterialType;
	char kFilamentMaterialTypeStr[32];

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
	char wFilament1Color[32];
	char wFilament2SerialNumber[64]; //16
	char wFilament2Color[32];

	int zOffset; // from getZOffset()
	bool zOffsetSet; // is zOffset vaild

	char GLastUsed[64];

	int LExtruderCount;
	int LPrinterLifetimePowerOnTime_min;
	int LPrinterLastPowerOnTime_min;
	int LExtruderLifetimePowerOnTime_min;
	// I suspect there are two of these on a dual color pinter

	int OBedTargetTemp_C;

	char VString[64]; // unknown V:version number

	char WSSID[64];				// network name
	char WBSSID[64];			// mac address of router
	char WChannel[64];			// wifi channel 1-14
	char WRssiValue[64];		// signal strength 0-100?
	char WPHY[64];				// network type
	char WSecurity[64];			// network encryption mode

	int XNozzleID;
	float XNozzleDiameter_mm;
	bool XNozzleIsLaser;
	char XNozzle1SerialNumber[64]; //32
	char XNozzle2SerialNumber[64]; //32

	char N4NetIP[64];			// ip address on machine
	char N4NetSSID[64];			// network name
	char N4NetChan[64];			// wifi channel 1-14
	char N4NetMAC[64];			// mac address of machine, basically a unique id
	int N4NetRssiValue;			// signal strength -100 dB to 0 dB
	int N4NetRssiValuePct;		// signal strength as a percent
};

// info on each printer type
struct XYZPrinterInfo
{
	// string constants
	const char *modelNum;	// how machine identifies itself over serial port
	const char *fileNum;	// used when creating .3w file for this printer
	const char *serialNum;	// first part of serial number
	const char *webNum;		// used when querying website for firmware

	// file format properties
	bool fileIsV5;			// v2 is 'normal' file format, v5 is for 'pro' systems
	bool fileIsZip;			// older file format, zips header
	bool comIsV3;			// old or new serial protocol, v3 is new

	// machine capabilities
	bool hasTuningMenu;		// has lcd tuning menu on printer
	bool hasHeatedBed;		// has heated bed
	bool dualExtruders;		// has two extruders
	bool hasWifi;			// has wifi support
	bool hasScanner;		// has built in 3d scanner
	bool supportsLaser;		// supports optional laser engraver

	// misc machine info, fill in at some point
	//bool hasLEDs;
	//bool hasCamera; // can we access this?

	// build volume
	int length;
	int width;
	int height;

	const char *screenName;	// user friendly name of device
};

struct XYZPrinterLangSt
{
	const char *abrv;
	const char *desc;
}; 

/*
 from xyprinting website

 Asia
   th		thailand
   ko-kr	korean
   ja-jp	japan
   cn		china
   zh-hk	hong kong
   zh-tw	taiwan
 Americas
   en-us	english
   la-pt-br	portuguese
   la-es	spanish
 Europe
   en-gb	english
   fr-fr	french
   de-de	german
   nl-nl	duch
   es-es	spanish
   it-it	italian
   ru-ru	russian
   pt-pt	portuguese
 Global
   global-en	english

 From language packs
   de	german
   en	english
   es	spanish
   fr	french
   gb	chinese
   it	italian
   ja	japanese
   kr	korean
   pt	portuguese
   ru	russian
   zh	chinese
   
*/
const int XYZPrintingLangCount = 9;
const XYZPrinterLangSt XYZPrintingLang[XYZPrintingLangCount] = 
{
	{ "en", "English" },
	{ "fr", "French" },
	{ "it", "Italian" },
	{ "de", "German" },
	{ "es", "Spanish" },
	{ "jp", "Japanese" },
	// dont really know if these exist
	// found refference to them in pdf document
	{ "tw", "Taiwanese" }, // Guoyu?
	{ "cn", "Chinese" }, // Mandarin?
	{ "kr", "Korean" },
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

	// attach to a serial stream
	// pass in NULL to disconnect from stream
	// old stream is returned so you can deal with closing it
	void setStream(Stream *s);
	bool isStreamSet() { return m_stream != NULL && m_stream->isOpen(); }

	// === query commands ===

	// update status struct
	// a updates all status, b-z updates individual status entries
	bool queryStatus(bool doPrint = false, float timeout_s = -1, char q1 = 'a', char q2 = 0, char q3 = 0, char q4 = 0);
	const XYZPrinterStatus* getPrinterStatus() { return &m_status; }
	const XYZPrinterInfo* getPrinterInfo() { return m_info; }

	// === action commands ===

	// get progress of action
	int getProgress();
	void setState(int state, float timeout_s = -1);

	// run auto bed leveling routine
	void calibrateBedStart(); 
	bool calibrateBedRun();

	bool calibrateBedPromptToLowerDetector();
	void calibrateBedDetectorLowered();

	bool calibrateBedPromptToRaiseDetector();
	void calibrateBedDetectorRaised();

	// heats up nozzle so you can clean it out with a wire
	void cleanNozzleStart();
	bool cleanNozzleRun();

	bool cleanNozzlePromtToClean();
	void cleanNozzleCancel(); 

	// home printer so you can safely move head
	void homePrinterStart();
	bool homePrinterRun();

	// move head in given direction, axis is one of x,y,z
	void jogPrinterStart(char axis, int dist_mm);
	bool jogPrinterRun();

	// loads filament
	void loadFilamentStart();
	bool loadFilamentRun();

	bool loadFilamentPromptToFinish();
	void loadFilamentCancel();

	// unloads filament without any user interaction
	void unloadFilamentStart();
	bool unloadFilamentRun();

	void unloadFilamentCancel();

	// === config commands ===

	// increment/decrement z offset by one step (1/100th of  a mm?)
	int incrementZOffset(bool up);
	int getZOffset(); // in hundredths of a mm
	bool setZOffset(int offset);

	bool restoreDefaults();
	bool setBuzzer(bool enable);
	bool setAutoLevel(bool enable);
	bool setLanguage(const char *lang); //lang is one of en, fr, it, de, es, jp
	bool setEnergySaving(int level); // level is 0-9 minutes till lights turn off?
	bool sendDisconnectWifi(); // send before setWifi?
	bool sendEngraverPlaceObject(); //****FixMe, what does this do?
	bool setMachineLife(int time_s); //****FixMe, what does this do?
	bool setMachineName(const char *name);
	bool setWifi(const char *ssid, const char *password, int channel);

	// === upload commands ===

	bool cancelPrint();
	bool pausePrint();
	bool resumePrint();
	bool readyPrint(); // call to notify printer bed is clear

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
	static const XYZPrinterInfo* serialToInfo(const char *serialNum);
	static const char* serialToName(const char *serialNum);

protected:

	// helper functions that upload a firmware or 3w file
	bool sendFileInit(const char *path, bool isPrint);
	bool sendFileProcess();
	bool sendFileFinalize();
	// how far allong the upload is
	float getFileUploadPct();

	bool serialSendMessage(const char *format, ...);

	struct sendFileData
	{
		bool isPrintActive;

		// helper variables
		int blockSize;
		int blockCount;
		int lastBlockSize;
		int curBlock;

		const char* data;	// data to be printed
		char *blockBuf;		// buffer to hold one block of data to be sent
	} pDat;

	XYZPrintStateCode translateStatus(int oldStatus);
	int translateErrorCode(int code);
	const char* stateCodesToStr(int state, int subState);
	static float nozzleIDToDiameter(int id);
	static bool nozzleIDIsLaser(int id);
	const char* errorCodeToStr(int code);
	const char* filamentMaterialTypeToStr(int materialType);
	const char* filamentColorIdToStr(int colorId);
	int rssiToPct(int rssi);
	bool findJsonVal(const char *str, const char *key, char *val);

	// serial functions

	//****FixMe, try to eliminate all these wait functions
	const char* waitForLine(float timeout_s = -1);
	bool waitForEndCom();
	bool waitForConfigOK(bool endCom = true, float timeout_s = -1);

	const char* checkForLine();
	bool checkForConfigOK(bool endCom = true);
	bool checkForJsonVal(const char *key, const char *val);
	bool checkForState(int state, int substate = -1, bool isSet = true);

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

	bool parseStatusSubstring(const char *str);

	bool isWIFI();

	Stream *m_stream;
	XYZPrinterStatus m_status;
	const XYZPrinterInfo *m_info;
	float m_runTimeout;

	static const int m_infoArrayLen = 21;
	static const XYZPrinterInfo m_infoArray[m_infoArrayLen];

	//ActState m_actState; //****FixMe, work this out
	int m_progress;
	msTimeout m_timeout;

	char m_jogAxis;
	int m_jogDist_mm;
};

#endif // XYZV3_H