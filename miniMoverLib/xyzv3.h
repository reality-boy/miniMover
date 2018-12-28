#ifndef XYZV3_H
#define XYZV3_H

#ifndef MAX_PATH
# define MAX_PATH 260
#endif

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
		// sub 42 - ???
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

	char vOsFirmwareVersion[64]; //10
	char vAppFirmwareVersion[64]; //10
	char vFirmwareVersion[64]; //10

	int wFilamentCount;
	char wFilament1SerialNumber[64]; //16
	char wFilament1Color[32];
	char wFilament2SerialNumber[64]; //16
	char wFilament2Color[32];

	int zOffset; // from getZOffsetStart()
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

const int XYZPrintingLangCount = 6; // 10;
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
	// the frontend now supports 12 languages
	//{ "pt", "Portuguese" },
	//{ "tw", "Taiwanese" }, // Guoyu?
	//{ "cn", "Chinese" }, // Mandarin?
	//{ "kr", "Korean" },
	//{ "ru", "Russian" },
	// Chinese simplified (zh-Hans)
	// Chinese traditional (zh-Hant)
};

// master class
class XYZV3
{
public:
	XYZV3();
	~XYZV3(); 

	// === serial communication ===

	// attach to a serial stream
	// pass in NULL to disconnect from stream
	// old stream is returned so you can deal with closing it
	void setStream(StreamT *s);
	const StreamT* getStream();
	bool isStreamSet() { return m_stream != NULL /*&& m_stream->isOpen()*/; }
	bool isWIFI();

	void doProcess();
	bool isInProgress();
	bool isSuccess();

	const char* getStateStr();
	int getProgressPct();

	// === query commands ===

	// update status struct
	// a updates all status, b-z updates individual status entries
	void queryStatusStart(bool doPrint = false, const char *s = "a");
	const XYZPrinterStatus* getPrinterStatus() { return &m_status; }
	const XYZPrinterInfo* getPrinterInfo() { return m_info; }

	// === action commands ===
	//****Note, currently only supported on mini and micro printers

	// run auto bed leveling routine
	void calibrateBedStart(); 
	bool calibrateBedPromptToLowerDetector();
	void calibrateBedDetectorLowered();

	bool calibrateBedPromptToRaiseDetector();
	void calibrateBedDetectorRaised();

	// heats up nozzle so you can clean it out with a wire
	void cleanNozzleStart();
	bool cleanNozzlePromtToClean();
	void cleanNozzleCancel(); 

	// home printer so you can safely move head
	void homePrinterStart();

	// move head in given direction, axis is one of x,y,z
	void jogPrinterStart(char axis, int dist_mm);

	// loads filament
	void loadFilamentStart();
	bool loadFilamentPromptToFinish();
	void loadFilamentCancel();

	// unloads filament without any user interaction
	void unloadFilamentStart();
	void unloadFilamentCancel();

	// === config commands ===
	//****Note, currently only supported on mini and micro printers

	// increment/decrement z offset by one step (1/100th of  a mm?)
	void incrementZOffsetStart(bool up); // z-offset stored in XYZPrinterStatus
	void getZOffsetStart(); // z-offset stored in XYZPrinterStatus
	void setZOffsetStart(int offset);
	void restoreDefaultsStart();
	void setBuzzerStart(bool enable);
	void setAutoLevelStart(bool enable);
	void setLanguageStart(const char *lang); //lang is one of en, fr, it, de, es, jp
	void setEnergySavingStart(int level); // level is 0-9 minutes till lights turn off?
	void sendDisconnectWifiStart(); // send before setWifiStart
	void sendEngraverPlaceObjectStart(); //****FixMe, what does this do?
	void setMachineLifeStart(int time_s); //****FixMe, what does this do?
	void setMachineNameStart(const char *name);
	//****Note, call sendDisconnectWifiStart() before calling this
	void setWifiStart(const char *ssid, const char *password, int channel);

	// === upload commands ===

	// print a gcode or 3w file, convert as needed
	void printFileStart(const char *path);

	// only work when print is in progress, after uploading
	void cancelPrint();
	void pausePrint();
	void resumePrint();
	void readyPrint(); // call to notify printer bed is clear

	// upload a new firmware to printer
	// probably not a good idea to mess with this!
	void uploadFirmwareStart(const char *path);

	// === file i/o ===

	// convert from gcode to 3w or back depending on file type
	// infoIdx is the index into the m_infoArray or -1 for auto
	void convertFileStart(const char *inPath, const char *outPath = NULL, int infoIdx = -1);

	bool isGcodeFile(const char *path);
	bool is3wFile(const char *path);

	// === machine info ===

	static int getInfoCount();
	static const XYZPrinterInfo* indexToInfo(int index);
	static const XYZPrinterInfo* modelToInfo(const char *modelNum);
	static const XYZPrinterInfo* serialToInfo(const char *serialNum);
	static const char* serialToName(const char *serialNum);

	// === misc commands ===

	// only currently supported on the 1.1 plus (and unreleased 2.1 plus?)
	bool captureImage(const char *path) { return V2W_CaptureImage(path); }

protected:

	void init();
	void startMessage();
	void endMessage();

	void setState(int/*ActState*/ state, float timeout_s = -1, bool needsMachineState = false);
	int /*ActState*/ getState();

	void setSubState(int/*ActState*/ subState, float timeout_s = -1);
	int /*ActState*/ getSubState();

	void calibrateBedRun();
	void cleanNozzleRun();
	void homePrinterRun();
	void jogPrinterRun();
	void loadFilamentRun();
	void unloadFilamentRun();
	void convertFileRun();
	void printFileRun();
	void uploadFirmwareRun();
	void queryStatusRun();
	void simpleCommandStart(float timeout_s, bool getZOffset, const char *format, ...);
	void simpleCommandRun();

	// substate, so it can run inside of other states
	void queryStatusSubStateStart(bool doPrint = false, const char *s = "a");
	bool queryStatusSubStateRun();

	// convert a gcode file to 3w format
	// infoIdx is the index into the m_infoArray or -1 for auto
	bool encryptFile(const char *inPath, const char *outPath = NULL, int infoIdx = -1);

	// convert a 3w file to gcode
	bool decryptFile(const char *inPath, const char *outPath = NULL);

	// helper functions that upload a firmware or 3w file
	bool sendFileInit(const char *path, bool isPrint);
	bool sendFileProcess();
	bool sendFileFinalize();
	// how far allong the upload is
	float getFileUploadPct();

	// on wifi we need a delay between sending messages or we overwhelm the connection.
	bool serialCanSendMessage();
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
	bool findJsonVal(const char *str, const char *key, char *val, int maxLen);

	void parseStatusSubstring(const char *str);

	// serial functions

	//****FixMe, try to eliminate these wait functions
	bool waitForEndCom(); // only blocks for 1/10th of a second
	bool waitForConfigOK(); // blocks for up to 5 seconds!

	const char* checkForLine();
	bool checkForJsonState(char *val, int maxLen);
	bool jsonValEquals(const char *tVal, const char *val);
	bool checkForState(int state, int substate = -1);
	bool checkForNotState(int state, int substate = -1);

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

	// encryptFile
	bool processGCode(const char *gcode, const int gcodeLen, const char *fileNum, char **processedBuf, int *headerLen, int *totalLen);
	bool encryptHeader(const char *gcode, int gcodeLen, bool fileIsV5, char **headerBuf, int *headerLen);
	bool encryptBody(const char *gcode, int gcodeLen, bool fileIsZip, char **bodyBuf, int *bodyLen);
	bool writeFile(FILE *fo, bool fileIsV5, bool fileIsZip, const char *headerBuf, int headerLen, char *bodyBuf, int bodyLen);

	// decryptFile

	StreamT *m_stream;
	XYZPrinterStatus m_status;
	const XYZPrinterInfo *m_info;
	float m_runTimeout;
	bool m_useV2Protocol;
	char m_V2Token[40];			// v2 wifi protocol returns a token to match a print job to a command


	static const int m_infoArrayLen = 24;
	static const XYZPrinterInfo m_infoArray[m_infoArrayLen];

	int /*ActState*/ m_actState;
	int /*ActState*/ m_actSubState;
	bool m_needsMachineState;
	int m_progress;
	msTimeout m_timeout;
	msTimeout m_sDelay;
	float m_timeout_s;
	char m_scCommand[512];
	bool m_scGetZOffset;
	bool m_doPrint;
	char m_qStr[5];
	bool m_qRes[5];

	char m_jogAxis;
	int m_jogDist_mm;
	int m_infoIdx;
	bool m_fileIsTemp;
	char m_filePath[MAX_PATH];
	char m_fileOutPath[MAX_PATH];

	const static int m_bodyOffset = 8192;

protected:

	//****Note, this is all experimental, so I left it blocking for now
	// that makes it simpler to reconfigure while we work out the details

	// helper function
	const char* waitForLine();
	bool waitForResponse(const char *response);
	int waitForInt(const char *response);
	const char* findValue(const char *str, const char *key);
	bool fillBuffer(char *buf, int len);

	//v2 serial protocol
	enum v2sFileMode
	{
		V2S_FILE,
		V2S_10_FW, // v1.0 printer (and 2.0?)
		V2S_11_FW_ENGINE, // v1.1 printer, engine fw
		V2S_11_FW_APP, // v1.1 printer, app fw
		V2S_11_FW_OS, // v1.1 printer, os fw
	};

	void V2S_queryStatusStart(bool doPrint);
	void V2S_parseStatusSubstring(const char *str, bool doPrint);
	void V2S_SendFirmware(const char* buf, int len);
	void V2S_SendFile(const char* buf, int len);
	void V2S_SendFileHelper(const char* buf, int len, v2sFileMode mode);

	// v2 wifi protocol
	void V2W_queryStatusStart(bool doPrint, const char *s = "all");
	// can't send firmware over wifi?
	void V2W_SendFile(const char* buf, int len);
	// wifi only commands?
	bool V2W_CaptureImage(const char *path);
	void V2W_PausePrint();
	void V2W_ResumePrint();
	void V2W_CancelPrint();
};

#endif // XYZV3_H