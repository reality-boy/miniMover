#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
# include <SDKDDKVer.h>
# include <Windows.h>
# include <conio.h> 
# pragma warning(disable:4996) // live on the edge!
#else
# include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "timer.h"
#include "serial.h"
#include "network.h"
#include "debug.h"
#include "xyzv3.h"

//****FixMe, move somewhere else
#ifndef _WIN32
//-------------------
// from https://jakash3.wordpress.com/2011/12/23/conio-h-for-linux/
 
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
 
// Set cursor position
void gotoxy(int x, int y) { printf("\x1B[%d;%df", y, x); }
 
// Clear terminal screen and set cursor to top left
void clrscr() { printf("\x1B[2J\x1B[0;0f"); }
 
void setTerminal(bool useBuf, bool doEcho) 
{
	struct termios oldt, newt;

	tcgetattr(0, &oldt);
	newt = oldt;

	// turn terminal line buffering on or off
	// on waits for enter before returning data
	if (!useBuf) 
		newt.c_lflag &= ~ICANON;
	else 
		newt.c_lflag |= ICANON;

	// turn terminal keyboard echo on or off
	if (!doEcho) 
		newt.c_lflag &= ~ECHO;
	else 
		newt.c_lflag |= ECHO;

	tcsetattr(0, TCSANOW, &newt);
}
 
// Get next immediate character input (no echo)
int getch() 
{
	setTerminal(false, false);
	int ch = getchar();
	setTerminal(true, true);

	return ch;
}
 
// Get next immediate character input (with echo)
int getche() 
{
	setTerminal(false, true);
	int ch = getchar();
	setTerminal(true, true);

	return ch;
}
 
// Check if a key has been pressed at terminal
int kbhit() 
{
	setTerminal(false, false);

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	fd_set fds;
	FD_ZERO(&fds);

	FD_SET(0, &fds);
	select(1, &fds, 0, 0, &tv);
	int ret = FD_ISSET(0, &fds);

	setTerminal(true, true);

	return ret;
}
//-------------------
#endif

// comment out to try network connection
#define USE_SERIAL

XYZV3 xyz;
Serial serial;
Socket soc;
char deviceName[SERIAL_MAX_DEV_NAME_LEN] = "";

void updateStatus(float pct)
{
	//****FixMe, could output dots based on pct complete
	(void)pct;

	// notify the user that we are still uploading
	static int printCount = 0;
	if(printCount % 4 == 0)
		printf(".");
	printCount++;
}

void postHelp()
{
	printf("miniMover v0.9\n");
	printf("usage: miniMover <args>\n");
	printf("  -? - print help message\n");
	printf("  -a+ - enable auto level\n");
	printf("  -a- - disable auto level\n");
	printf("  -b+ - enable buzzer\n");
	printf("  -b- - disable buzzer\n");
	printf("  -cl - clean nozzle\n");
	printf("  -ca - calibrate bed\n");
	printf("  -c file - convert file\n");
	printf("  -f file - upload firmware, experimental!\n");
	printf("  -h - home printer\n");
	printf("  -l - load filament\n");
	printf("  -o num - increment/decrement z offset by num\n");
	printf("  -po devName - set serial port device name, -1 auto detects port\n");
	printf("  -p file - print file\n");
	printf("  -r - print raw status\n");
	printf("  -s - print status\n");
	printf("  -u - unload filament\n");
	printf("  -x num - jog x axis by num, or 10 if num not provided\n");
	printf("  -y num - jog y axis by num, or 10 if num not provided\n");
	printf("  -z num - jog z axis by num, or 10 if num not provided\n");
	printf("  file - print file if .gcode, otherwise convert to gcode if .3w\n");
}

bool checkCon()
{
	//****FixMe, is this called frequently?
	const char *tDevice = deviceName;
	if(!deviceName[0])
	{
		int id = SerialHelper::queryForPorts("XYZ");
		tDevice = SerialHelper::getPortDeviceName(id);
	}
#ifdef USE_SERIAL
	if(tDevice && serial.openSerial(tDevice, 115200))
	{
		xyz.setStream(&serial);
		return true;
	}
#else // network
	const char *ip = "192.168.1.118";
	int port = 9100;
	if(soc.openSocket(ip, port))
	{
		xyz.setStream(&soc);
		return true;
	}
#endif

	xyz.setStream(NULL);
	if(tDevice)
		printf("printer not found on port: %s\n", tDevice);
	else
		printf("printer not found\n");

	return false;
}

// accepts a 3w file formatted for this printer

bool isKey(const char *str)
{
	return (str && str[0] == '-' && !isdigit(str[1]));
}

bool printStatus()
{
	if(xyz.queryStatus())
	{
		printf("\nPrinter Status\n\n");
		const XYZPrinterStatus *st = xyz.getPrinterStatus();
		const XYZPrinterInfo *inf = xyz.getPrinterInfo();
		if(st->isValid)
		{
			printf("Bed temperature: %d C, target temp: %d C\n", 
				st->bBedActualTemp_C, st->OBedTargetTemp_C);

			printf("Bed calibration\n");
			printf(" %d, %d, %d\n", st->cCalib[0], st->cCalib[1], st->cCalib[2]);
			printf(" %d, %d, %d\n", st->cCalib[3], st->cCalib[4], st->cCalib[5]);
			printf(" %d, %d, %d\n", st->cCalib[6], st->cCalib[7], st->cCalib[8]);

			if(st->dPrintPercentComplete == 0 && st->dPrintElapsedTime_m == 0 && st->dPrintTimeLeft_m == 0)
				printf("No job running\n");
			else
				printf("Printer status: %d %% %d m %d m\n", st->dPrintPercentComplete, st->dPrintElapsedTime_m, st->dPrintTimeLeft_m);

			if(st->eErrorStatus != 0)
				printf("Error: (0x%08x)%s\n", st->eErrorStatus, st->eErrorStatusStr);

			printf("Serial number: %s\n", st->iMachineSerialNum);

			printf("Status: (%d, %d) %s\n", st->jPrinterState, st->jPrinterSubState, st->jPrinterStateStr);

			printf("language: %s\n", st->lLang);

			if(st->mVal[0] != 0)
				printf("mVal: %d,%d,%d\n", st->mVal[0], st->mVal[1], st->mVal[2]);

			printf("printer name: %s\n", st->nMachineName);

			printf("Package size: %d bytes\n", st->oPacketSize);
			printf("ot: %d bytes\n", st->oT);
			printf("oc: %d bytes\n", st->oC);
			printf("Auto level %s\n", (st->oAutoLevelEnabled) ? "enabled" : "disabled");

			printf("Model number: %s\n", st->pMachineModelNumber);

			printf("Buzzer: %s\n", (st->sBuzzerEnabled) ? "enabled" : "disabled");
			printf("Fd: %d\n", st->sFd);
			printf("Fm: %d\n", st->sFm);
			if(st->sButton)
				printf("Has button\n");
			if(st->sFrontDoor)
				printf("Front door open\n");
			if(st->sTopDoor)
				printf("Top door open\n");
			if(st->sHasLazer)
				printf("Has lazer\n");
			if(st->sSDCard)
				printf("Has sd card\n");

			if(st->tExtruderCount > 1 && st->tExtruder2ActualTemp_C > 0)
				printf("Extruder temp: %d C, %d C, target temp: %d C\n", st->tExtruder1ActualTemp_C, st->tExtruder2ActualTemp_C, st->tExtruderTargetTemp_C);
			else
				printf("Extruder temp: %d C, target temp: %d C\n", st->tExtruder1ActualTemp_C, st->tExtruderTargetTemp_C);

			printf("Firmware version: %s\n", st->vFirmwareVersion);

			printf("Filament SN: %s\n", st->wFilament1SerialNumber);

			if(st->wFilament1Color[0])
				printf("Filament color: %s\n", st->wFilament1Color);

			if(st->wFilamentCount > 1 && st->wFilament2SerialNumber[0])
				printf("Filament 2 SN: %s\n", st->wFilament2SerialNumber);

			if(st->wFilamentCount > 1 && st->wFilament2Color[0])
				printf("Filament 2 color: %s\n", st->wFilament2Color);

			if(st->sOpenFilament)
				printf("Uses open filament\n");

			if(st->fFilamentSpoolCount > 1 && st->fFilament2Remaining_mm > 0)
				printf("Filament length: %0.3f m, %0.3f m\n", st->fFilament1Remaining_mm / 1000.0f, st->fFilament2Remaining_mm / 1000.0f);
			else 
				printf("Filament length: %0.3f m\n", st->fFilament1Remaining_mm / 1000.0f);

			printf("PLA filament: %d\n", st->hIsFilamentPLA);

			if(st->kFilamentMaterialType > 0)
				printf("Filament material type: (%d) %s\n", st->kFilamentMaterialType, st->kFilamentMaterialTypeStr);

			if(st->GLastUsed[0])
				printf("Filament last used: %s m\n", st->GLastUsed);

			printf("Z-Offset %d\n", st->zOffset);

			printf("Lifetime power on time: %d min\n", st->LPrinterLifetimePowerOnTime_min);
			if(st->LPrinterLastPowerOnTime_min > 0)
				printf("Last print time: %d min\n", st->LPrinterLastPowerOnTime_min);
			printf("Extruder total time: %d min\n", st->LExtruderLifetimePowerOnTime_min);
			// maybe a second extruder timer?

			if(st->VString[0])
				printf("v string: %s\n", st->VString);

			if(st->WSSID[0])
			{
				printf("Wireless SSID: %s\n", st->WSSID);
				printf("Wireless BSSID: %s\n", st->WBSSID);
				printf("Wireless Channel: %s\n", st->WChannel);
				printf("Wireless RssiValue: %s\n", st->WRssiValue);
				printf("Wireless PHY: %s\n", st->WPHY);
				printf("Wireless Security: %s\n", st->WSecurity);
			}

			printf("nozzle: %0.2f laser %d mm sn: %s", st->XNozzleDiameter_mm, st->XNozzleIsLaser, st->XNozzle1SerialNumber);
			if(st->XNozzle2SerialNumber[0])
				printf(", %s", st->XNozzle2SerialNumber);
			printf("\n");

			if(st->N4NetSSID[0])
			{
				printf("Wireless SSID: %s\n", st->N4NetSSID);
				printf("Wireless IP: %s\n", st->N4NetIP);
				printf("Wireless Channel: %s\n", st->N4NetChan);
				printf("Wireless MAC: %s\n", st->N4NetMAC);
				printf("Wireless RssiValue: %d dB - %d %%\n", st->N4NetRssiValue, st->N4NetRssiValuePct);
			}

			if(inf)
			{
				// model number is redundant, printed above

				printf("File id: %s\n", inf->fileNum);
				printf("Machine serial number: %s\n", inf->serialNum);
				printf("Machine screen name: %s\n", inf->screenName);

				printf("isV5: %d, isZip: %d, comV3: %d\n",
							inf->fileIsV5, // v2 is 'normal' file format, v5 is for 'pro' systems
							inf->fileIsZip, // older file format, zips header
							inf->comIsV3); // old or new serial protocol, v3 is new

				// build volume
				printf("Printer build volume: %d L %d W %d H\n", inf->length, inf->width, inf->height);
			}
		}
		// else do nothing

		return true;
	}

	return false;
}

bool monitorPrintJob()
{
	if(xyz.queryStatus())
	{
		const XYZPrinterStatus *st = xyz.getPrinterStatus();
		if(st->isValid)
		{
			printf("S: %s", st->jPrinterStateStr);

			if(st->tExtruderCount > 1 && st->tExtruder2ActualTemp_C > 0)
				printf(", temp: %d C %d C / %d C", st->tExtruder1ActualTemp_C, st->tExtruder2ActualTemp_C, st->tExtruderTargetTemp_C);
			else
				printf(", temp: %d C / %d C", st->tExtruder1ActualTemp_C, st->tExtruderTargetTemp_C);

			if(st->bBedActualTemp_C > 30)
				printf(" - %d C / %d C", 
					st->bBedActualTemp_C,
					st->OBedTargetTemp_C);

			if(st->dPrintPercentComplete != 0 || st->dPrintElapsedTime_m != 0 || st->dPrintTimeLeft_m != 0)
				printf(" Job: %d %% %d m %d m", st->dPrintPercentComplete, st->dPrintElapsedTime_m, st->dPrintTimeLeft_m);

			if(st->eErrorStatus != 0)
				printf(" Error: (0x%08x)%s", st->eErrorStatus, st->eErrorStatusStr);

			printf("\n");
		}
		// else just wait till next cycle

		// if not PRINT_NONE
		return ( st->jPrinterState != PRINT_ENDING_PROCESS_DONE &&
				 st->jPrinterState != PRINT_NONE);
	}

	return true; // assume we are still printing if we got an update error
}

bool handlePrintFile(const char *path)
{
	if(path && checkCon())
	{
		if(xyz.printFile(path, updateStatus))
		{
			printf("\n");
			// check status and wait for user to pause/cancel print
			bool isPrintPaused = false;
			int count = 0;

			while(true)
			{
				count++;
				// check if they want to cancel or pause
				if(kbhit())
				{
					// get first key
					char c = (char)getch();

					// eat any other keys
					while(kbhit())
						getch();

					// hit space to toggle pause on and off
					if(c == ' ')
					{
						if(isPrintPaused)
							xyz.resumePrint();
						else
							xyz.pausePrint();
						isPrintPaused = !isPrintPaused;
					}
					else // any other key cancels print job
					{
						xyz.cancelPrint();
						break;
					}
				}

				// update status and quit if done
				if(count % 500 == 0)
				{
					if(!monitorPrintJob())
					{
						break;
					}
				}

#ifdef _WIN32
				Sleep(10); // don't spin too fast
#else
				usleep(10 * 1000);
#endif
			}

			return true;
		}
	}

	return false;
}

//#define DUMP_STATUS

int main(int argc, char **argv)
{
	debugInit();
	msTimer tm;

	if(argc <= 1)
	{
#ifdef DUMP_STATUS
		if(checkCon())
			xyz.queryStatus(true);
#else
		postHelp();
#endif
	}
	else
	{
		for(int i=1; i<argc; i++)
		{
			if(!isKey(argv[i]))
			{
				// print or convert file depending on extension
				if(xyz.isGcodeFile(argv[i]))
				{
					printf("starting print file\n");
					if(handlePrintFile(argv[i]))
						printf("print file succeeded\n");
					else
						printf("print file failed to print %s\n", argv[i]);
				}
				else if(xyz.is3wFile(argv[i]))
				{
					printf("starting convert file\n");
					checkCon(); 
					if(xyz.convertFile(argv[i]))
						printf("convert file succeeded\n");
					else
						printf("convert file failed to convert %s\n", argv[i]);
				}
				else
					printf("unknown file type %s\n", argv[i]);
			}
			else
			{
				int t;

				switch(argv[i][1])
				{
				case '?':
					postHelp();
					break;
				case 'a':
					if(checkCon())
						xyz.setAutoLevel(argv[i][2] != '-');
					break;
				case 'b':
					if(checkCon())
						xyz.setBuzzer(argv[i][2] != '-');
					break;
				case 'c':
					if(argv[i][2] == 'l') // clean
					{
						if(checkCon())
						{
							printf("starting clean nozzle\n");
							if( xyz.cleanNozzleStart() &&
								xyz.cleanNozzleRun())
							{
								printf("clean nozzle with a wire and press enter when finished\n");
								getch();

								if(xyz.cleanNozzleCancel())
								{
									printf("clean nozzle succeeded\n");
								}
								else
									printf("clean nozzle failed\n");
							}
							else
								printf("clean nozzle failed\n");
						}
					}
					else if(argv[i][2] == 'a') // calibrate
					{
						if(checkCon())
						{
							printf("starting calibration\n");
							if( xyz.calibrateBedStart() &&
								xyz.calibrateBedDetectorLowered())
							{
								printf("lower detector and hit enter to continue...\n");
								getch();
								if(xyz.calibrateBedRun())
								{
									printf("raise detector and hit enter to continue...\n");
									getch();
									if(xyz.calibrateBedFinish())
									{
										printf("calibration completed succesfully\n");
									}
									else
										printf("calibration failed\n");
								}
								else
									printf("calibration failed\n");
							}
							else
								printf("calibration failed\n");
						}
					}
					else // convert
					{
						if(i+1 < argc && !isKey(argv[i+1])) 
						{
							printf("starting convert file\n");
							checkCon(); 
							if(xyz.convertFile(argv[i+1]))
								printf("convert file succeeded\n");
							else
								printf("convert file failed to convert %s\n", argv[i+1]);
							i++;
						}
						else
							printf("invalid argument\n");
					}
					break;
				case 'f':
					if(i+1 < argc && !isKey(argv[i+1])) 
					{
						if(checkCon())
						{
							printf("starting update firmware\n");
							if(xyz.writeFirmware(argv[i+1], updateStatus))
								printf("update firmware succeeded\n");
							else
								printf("update firmware failed to upload %s\n", argv[i+1]);
						}
						i++;
					}
					else
						printf("invalid argument\n");
					break;
				case 'h':
					if(checkCon())
					{
						printf("start home printer\n");
						if( xyz.homePrinterStart() &&
							xyz.homePrinterRun())
							printf("home printer succeeded\n");
						else
							printf("home printer failed\n");
					}
					break;
				case 'l':
					if(checkCon())
					{
						printf("starting load filament\n");
						if( xyz.loadFilamentStart() &&
							xyz.loadFilamentRun())
						{
							printf("wait for filament to come out of nozzle then hit enter\n");
							getch();

							if(xyz.loadFilamentCancel())
							{
								printf("load filament succeeded\n");
							}
							else
								printf("load filament failed\n");
						}
						else
							printf("load filament failed\n");
					}
					break;
				case 'o':
					if(i+1 < argc && !isKey(argv[i+1]))
					{
						t = atoi(argv[i+1]);
						if(checkCon())
						{
							int offset = xyz.getZOffset();
							if(offset > 0)
								xyz.setZOffset(offset + t);
						}
						i++;
					}
					else
						printf("invalid argument\n");
					break;
				case 'p': // set port
					if(argv[i][2] == 'o')
					{
						if(i+1 < argc && !isKey(argv[i+1])) 
						{
							strcpy(deviceName, argv[i+1]);
							// if -1 then zero out device name to indicate auto detect
							if(deviceName[0] == '-')
								deviceName[0] = '\0';
							i++;
						} else 
							printf("port needs a name\n");
					}
					else
					{
						if(i+1 < argc && !isKey(argv[i+1])) 
						{
							if(checkCon())
							{
								printf("starting print file\n");
								if(handlePrintFile(argv[i+1]))
									printf("print file succeeded\n");
								else
									printf("print file failed to print %s\n", argv[i+1]);
							}
							i++;
						}
						else
							printf("invalid argument\n");
					}
					break;
				case 'r': // raw status
					if(checkCon())
						xyz.queryStatus(true);
					break;
				case 's': // status
					if(checkCon())
						printStatus();
					break;
				case 'u':
					if(checkCon())
					{
						printf("start unload filament\n");
						if( xyz.unloadFilamentStart() &&
							xyz.unloadFilamentRun())
							printf("unload filament succeeded\n");
						else
							printf("unload filament failed\n");
					}
					break;
				case 'x':
					t = 10;
					if(i+1 < argc && !isKey(argv[i+1])) 
						t = atoi(argv[i+1]);
					if(checkCon())
					{
						if( !xyz.jogPrinterStart('x', t) ||
							!xyz.jogPrinterRun())
							printf("jog printer failed\n");
					}
					i++;
					break;
				case 'y':
					t = 10;
					if(i+1 < argc && !isKey(argv[i+1]))
						t = atoi(argv[i+1]);
					if(checkCon())
					{
						if( !xyz.jogPrinterStart('y', t) ||
							!xyz.jogPrinterRun())
							printf("jog printer failed\n");
					}
					i++;
					break;
				case 'z':
					t = 10;
					if(i+1 < argc && !isKey(argv[i+1]))
						t = atoi(argv[i+1]);
					if(checkCon())
					{
						if( !xyz.jogPrinterStart('z', t) ||
							!xyz.jogPrinterRun())
							printf("jog printer failed\n");
					}
					i++;
					break;
				default:
					printf("unknown argument %s\n", argv[i]);
					break;

				//****FixMe, hook these up!
				// restoreDefaults()
				// setLanguage("en")
				// setEnergySaving(0-9)
				// sendDisconnectApp()
				// sendDisconnectApp()
				// setMachineLife(int time_s)
				// setMachineName("name")
				// setWifi(const char *ssid, const char *password, int channel)
				}
			}
		}
	}
	
	// disconnect just in case
	xyz.setStream(NULL);

#ifdef USE_SERIAL
	serial.closeSerial();
#else // network
	soc.closeSocket();
#endif

	debugFinalize();

#if defined(_DEBUG) |  defined(DUMP_STATUS)
	printf("run took %0.4f seconds\n", tm.stopTimer());
	printf("\nhit any key to continue.\n");
	getch();
#endif

	return 0;
}
