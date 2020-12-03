#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
# include <SDKDDKVer.h>
# include <Windows.h>
# include <conio.h> 
# pragma warning(disable:4996) // live on the edge!
#else
# include <unistd.h>
# define Sleep(t) usleep((t) * 1000)
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "timer.h"
#include "stream.h"
#include "serial.h"
#include "socket.h"
#include "debug.h"
#include "xyzv3.h"
#include "XYZPrinterList.h"

#ifndef _WIN32
// from https://jakash3.wordpress.com/2011/12/23/conio-h-for-linux/
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
 
void setTerminal(bool state) 
{
	struct termios newt;
	tcgetattr(0, &newt);

	if (!state) {
		newt.c_lflag &= ~ICANON;
		newt.c_lflag &= ~ECHO;
	} else {
		newt.c_lflag |= ICANON;
		newt.c_lflag |= ECHO;
	}

	tcsetattr(0, TCSANOW, &newt);
}
 
// Get next immediate character input (no echo)
int getch() 
{
	setTerminal(false);

	int ch = getchar();

	setTerminal(true);

	return ch;
}
 
// Check if a key has been pressed at terminal
int kbhit() 
{
	setTerminal(false);

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	select(1, &fds, 0, 0, &tv);
	int ret = FD_ISSET(0, &fds);

	setTerminal(true);

	return ret;
}
#endif

// comment out to try network connection

XYZV3 xyz;
Serial serial;
Socket soc;
char deviceName[SERIAL_MAX_DEV_NAME_LEN] = "";
WifiList g_wifiList;

void postHelp()
{
	printf("miniMover %s\n", g_ver);
	printf("usage: miniMover <args> [file]\n");
	printf("  file - print file if .gcode, otherwise convert to gcode if .3w\n");
	printf("  -d devName - set serial port device name or wifi ip address, -1 auto detects port\n");
	//printf("  -po devName - set serial port device name, -1 auto detects port\n");
	printf("  -c file - convert file\n");
	printf("  -p file - print file\n");
	printf("  -f file - upload firmware, experimental!\n");
	printf("  -r - print raw status\n");
	printf("  -s - print status\n");
	printf("  -? - print help message\n");
	printf("\n");
	printf(" * The following only work on machines that lack an LCD *\n");
	printf("\n");
	printf("  -a+ - enable auto level\n");
	printf("  -a- - disable auto level\n");
	printf("  -b+ - enable buzzer\n");
	printf("  -b- - disable buzzer\n");
	printf("  -cl - clean nozzle\n");
	printf("  -ca - calibrate bed\n");
	printf("  -h - home printer\n");
	printf("  -l - load filament\n");
	printf("  -o num - increment/decrement z offset by num\n");
	printf("  -u - unload filament\n");
	printf("  -x num - jog x axis by num, or 10 if num not provided\n");
	printf("  -y num - jog y axis by num, or 10 if num not provided\n");
	printf("  -z num - jog z axis by num, or 10 if num not provided\n");
	printf("  -i path - snap image from internal camera on 1.1 plus machines\n");
}

void doProcessWithSleep(int ms = 10)
{
	Sleep(ms);
	xyz.doProcess();
}

bool checkCon()
{
	if(!xyz.isStreamSet())
	{
		const char *tDevice = deviceName;

		// if name not set, auto detect, 
		//****FixMe, could be a lot more intelligent than just trying 
		// the first printer found in the list

		// try serial first
		if(!tDevice || !tDevice[0])
			tDevice = SerialHelper::getPortDeviceName(SerialHelper::queryForPorts("XYZ"));
		// then wifi
		if(!tDevice || !tDevice[0])
			if(g_wifiList.m_count > 0) // try wifi
				tDevice = g_wifiList.m_list[0].m_ip;

		if(tDevice && tDevice[0])
		{
			if(StreamT::isNetworkAddress(tDevice))
			{
				//****FixMe, add support for custom port by appending :9100 to end of ip like 192.168.1.118:9100
				// this will allow us to support a custom wifi to usb adapter
				if(soc.openStream(tDevice, 9100))
				{
					xyz.setStream(&soc);

					// force a status update if new connection
					if(xyz.isStreamSet())
					{
						xyz.queryStatusStart();
						while(xyz.isInProgress())
							doProcessWithSleep();
					}

					return true;
				}
			}
			else // serial port
			{
				if(serial.openStream(tDevice, 115200))
				{
					xyz.setStream(&serial);

					// force a status update if new connection
					if(xyz.isStreamSet())
					{
						xyz.queryStatusStart();
						while(xyz.isInProgress())
							doProcessWithSleep();
					}

					return true;
				}
			}
		}

		// failed to find a printer, report to user
		xyz.setStream(NULL);
		if(tDevice)
			printf("printer not found on port: '%s'\n", tDevice);
		else
			printf("printer not found\n");

		return false;
	}

	return xyz.isStreamSet();
}

// accepts a 3w file formatted for this printer

bool isKey(const char *str)
{
	return (str && str[0] == '-' && !isdigit(str[1]));
}

bool printStatus()
{
	xyz.queryStatusStart();
	while(xyz.isInProgress())
		doProcessWithSleep();

	if(xyz.isSuccess())
	{
		printf("\nPrinter Status\n\n");
		const XYZPrinterStatus *st = xyz.getPrinterStatus();
		const XYZPrinterInfo *inf = xyz.getPrinterInfo();
		if(st->isValid)
		{
			printf("Bed temperature: %d C, target temp: %d C\n", 
				st->bBedActualTemp_C, st->OBedTargetTemp_C);

			printf("Bed calibration:\n");
			printf(" %d, %d, %d\n", st->cCalib[0], st->cCalib[1], st->cCalib[2]);
			printf(" %d, %d, %d\n", st->cCalib[3], st->cCalib[4], st->cCalib[5]);
			printf(" %d, %d, %d\n", st->cCalib[6], st->cCalib[7], st->cCalib[8]);

			if(st->dPrintPercentComplete == 0 && st->dPrintElapsedTime_m == 0 && st->dPrintTimeLeft_m == 0)
				printf("Printer status: No job running\n");
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
			printf("Auto level: %s\n", (st->oAutoLevelEnabled) ? "enabled" : "disabled");

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

			printf("Z-Offset: %d\n", st->zOffset);

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
	Sleep(5000);

	xyz.queryStatusStart();
	while(xyz.isInProgress())
		doProcessWithSleep();

	if(xyz.isSuccess())
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

			// if not PRINT_NONE
			return ( st->jPrinterState != PRINT_ENDING_PROCESS_DONE &&
					 st->jPrinterState != PRINT_NONE);
		}
		// else just wait till next cycle
	}

	return true; // assume we are still printing if we got an update error
}

bool handlePrintFile(const char *path)
{
	if(path && checkCon())
	{
		printf("uploading file to printer\n");

		xyz.printFileStart(path);
		int printCount = 0;
		while(xyz.isInProgress())
		{
			doProcessWithSleep();

			// notify the user that we are still uploading
			printCount++;
			if(printCount % 4 == 0)
				printf(".");
		}
		printf("\n");

		if(xyz.isSuccess())
		{
			printf("monitoring print\n");

			// check status and wait for user to pause/cancel print
			bool isPrintPaused = false;
			int count = 0;
			int interval = (xyz.isWIFI()) ? 2000 : 500;

			while(true)
			{
				// check if they want to cancel or pause
				if(kbhit())
				{
					// get first key
					char c = (char)getch();

					// eat any other keys
					while(kbhit())
					{
						getch();
					}

					// hit space to toggle pause on and off
					if(c == ' ')
					{
						if(isPrintPaused)
						{
							printf("resuming print\n");
							xyz.resumePrint();
						}
						else
						{
							printf("pausing print\n");
							xyz.pausePrint();
						}
						isPrintPaused = !isPrintPaused;
					}
					else // any other key cancels print job
					{
						printf("canceling print\n");
						xyz.cancelPrint();
						break;
					}
				}

				// update status and quit if done
				if(count % interval == 0)
				{
					if(!monitorPrintJob())
					{
						break;
					}
				}

				count++;
			}

			return true;
		}
	}

	return false;
}

//#define DUMP_STATUS

void runTimedTest(float delay_s)
{
	if(checkCon())
	{
		msTimer ts, tf;
		int f = 0; // failure count
		int t = 0; // total count
		int c = 0; // dot count
		int ct = 5; // dot count

		printf("looping test with %0.2f second delay\n\n", delay_s);

		while(true)
		{
			t++; // total count

			// time our command
			ts.startTimer();
			tf.startTimer();

			//xyz.queryStatusStart();
			xyz.homePrinterStart();
			while(xyz.isInProgress())
				doProcessWithSleep();

			// connection lost
			if(!xyz.isStreamSet())
			{
				tf.stopTimer();
				// insert newline if any . printed
				if(c>=ct) printf("\n");
				c = 0; // reset count if errors printed

				f++; // failure count
				printf("connection lost!\n");
				break;
			}
			// query failed
			else if(!xyz.isSuccess())
			{
				tf.stopTimer();
				// insert newline if any . printed
				if(c>=ct) printf("\n");
				c = 0; // reset count if errors printed

				f++; // failure count
				printf("failed to recieve! %d:%d %0.1f %%\n", f, t, 100.0f*(float)f/(float)t);
			}
			// success
			else
			{
				ts.stopTimer();
				// print . only if ct successes in a row
				c++;
				if(c % ct == 0)
					printf(".");
			}

			// and delay
			if(delay_s > 0)
				Sleep((int)(delay_s * 1000.0f));

			// bail on keyboard hit
			if(kbhit())
			{
				while(kbhit())
					getch();
				break;
			}
		}

		printf("\n");
		printf("failed %d out of %d times %0.1f %%\n", f, t, 100.0f*(float)f/(float)t);

		printf("\nrun time\n");
		printf("minTime: %0.2f\n", ts.getMinTime_s());
		printf("avgTime: %0.2f\n", ts.getAvgTime_s());
		printf("maxTime: %0.2f\n", ts.getMaxTime_s());

		if(f > 0)
		{
			printf("\nfail time\n");
			printf("minTime: %0.2f\n", tf.getMinTime_s());
			printf("avgTime: %0.2f\n", tf.getAvgTime_s());
			printf("maxTime: %0.2f\n", tf.getMaxTime_s());
		}

		if(xyz.getStream())
		{
			printf("\nmessage time\n");
			printf("minTime: %0.2f\n", xyz.getStream()->m_msgTimer.getMinTime_s());
			printf("avgTime: %0.2f\n", xyz.getStream()->m_msgTimer.getAvgTime_s());
			printf("maxTime: %0.2f\n", xyz.getStream()->m_msgTimer.getMaxTime_s());
		}
	}
}

int main(int argc, char **argv)
{
	debugInit();
	msTimer tm;

	// read cached list of known networked printers 
	g_wifiList.readWifiList();

	if(argc <= 1)
	{
#ifdef DUMP_STATUS
		if(checkCon())
		{
			xyz.queryStatusStart(true);
			while(xyz.isInProgress())
				doProcessWithSleep();
		}
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
						printf("print file failed to print '%s'\n", argv[i]);
				}
				else if(xyz.is3wFile(argv[i]))
				{
					printf("starting convert file\n");
					checkCon(); 
					xyz.convertFileStart(argv[i]);
					while(xyz.isInProgress())
						doProcessWithSleep(); //****FixMe, print progress

					if(xyz.isSuccess())
						printf("convert file succeeded\n");
					else
						printf("convert file failed\n");
				}
				else
					printf("unknown file type '%s'\n", argv[i]);
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
					{
						xyz.setAutoLevelStart(argv[i][2] != '-');
						while(xyz.isInProgress())
							doProcessWithSleep();
					}
					break;
				case 'b':
					if(checkCon())
					{
						xyz.setBuzzerStart(argv[i][2] != '-');
						while(xyz.isInProgress())
							doProcessWithSleep();
					}
					break;
				case 'c':
					if(argv[i][2] == 'l') // clean
					{
						if(checkCon())
						{
							printf("starting clean nozzle\n");
							xyz.cleanNozzleStart();
							while(xyz.isInProgress())
							{
								doProcessWithSleep();
								if(xyz.cleanNozzlePromtToClean())
								{
									printf("clean nozzle with a wire and press enter when finished\n");
									getch();

									xyz.cleanNozzleCancel();
								}
							}
							printf("clean nozzle succeeded\n");
						}
					}
					else if(argv[i][2] == 'a') // calibrate
					{
						if(checkCon())
						{
							printf("starting calibration\n");
							xyz.calibrateBedStart();
							while(xyz.isInProgress())
							{
								doProcessWithSleep();
								if(xyz.calibrateBedPromptToLowerDetector())
								{
									printf("lower detector and hit enter to continue...\n");
									getch();
									xyz.calibrateBedDetectorLowered();
								}
								else if(xyz.calibrateBedPromptToRaiseDetector())
								{
									printf("raise detector and hit enter to continue...\n");
									getch();
									xyz.calibrateBedDetectorRaised();
								}
							}
							printf("calibration completed succesfully\n");
						}
					}
					else // convert
					{
						if(i+1 < argc && !isKey(argv[i+1])) 
						{
							printf("starting convert file\n");
							xyz.convertFileStart(argv[i+1]);
							while(xyz.isInProgress())
								doProcessWithSleep(); //****FixMe, print progress

							if(xyz.isSuccess())
								printf("convert file succeeded\n");
							else
								printf("convert file failed\n");
							i++;
						}
						else
							printf("invalid argument\n");
					}
					break;
				case 'd':
					if(i+1 < argc && !isKey(argv[i+1])) 
					{
						// close old connection
						xyz.setStream(NULL);
						strcpy(deviceName, argv[i+1]);
						// if -1 then zero out device name to indicate auto detect
						if(deviceName[0] == '-')
							deviceName[0] = '\0';
						i++;
					} else 
						printf("device needs a name\n");
					break;
				case 'f':
					if(i+1 < argc && !isKey(argv[i+1])) 
					{
						if(checkCon())
						{
							printf("starting update firmware\n");
							xyz.uploadFirmwareStart(argv[i+1]);
							int printCount = 0;
							while(xyz.isInProgress())
							{
								doProcessWithSleep();
								// notify the user that we are still uploading
								if(printCount % 4 == 0)
									printf(".");
								printCount++;
							}
							if(xyz.isSuccess())
								printf("update firmware succeeded\n");
							else
								printf("update firmware failed\n");
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
						xyz.homePrinterStart();
						while(xyz.isInProgress())
							doProcessWithSleep(); // spin

						printf("home printer succeeded\n");
					}
					break;
				case 'l':
					if(checkCon())
					{
						printf("starting load filament\n");
						xyz.loadFilamentStart();
						while(xyz.isInProgress())
						{
							doProcessWithSleep();
							if(xyz.loadFilamentPromptToFinish())
							{
								printf("wait for filament to come out of nozzle then hit enter\n");
								getch();
								xyz.loadFilamentCancel();
							}
						}
						printf("load filament succeeded\n");
					}
					break;
				case 'o':
					if(i+1 < argc && !isKey(argv[i+1]))
					{
						t = atoi(argv[i+1]);
						if(checkCon())
						{
							xyz.queryStatusStart();
							while(xyz.isInProgress())
								doProcessWithSleep();

							if(xyz.isSuccess() && xyz.getPrinterStatus())
							{
								int offset = xyz.getPrinterStatus()->zOffset;
								if(offset > 0)
								{
									xyz.setZOffsetStart(offset + t);
									while(xyz.isInProgress())
										doProcessWithSleep();
								}
							}
						}
						i++;
					}
					else
						printf("invalid argument\n");
					break;
				case 'p': // set port
					if(argv[i][2] == 'o')
					{
						// deprecated, see -d instead
						if(i+1 < argc && !isKey(argv[i+1])) 
						{
							// close old connection
							xyz.setStream(NULL);
							strcpy(deviceName, argv[i+1]);
							// if -1 then zero out device name to indicate auto detect
							if(deviceName[0] == '-')
								deviceName[0] = '\0';
							i++;
						} else 
							printf("device needs a name\n");
					}
					else // print file
					{
						if(i+1 < argc && !isKey(argv[i+1])) 
						{
							if(checkCon())
							{
								printf("starting print file\n");
								if(handlePrintFile(argv[i+1]))
									printf("print file succeeded\n");
								else
									printf("print file failed to print '%s'\n", argv[i+1]);
							}
							i++;
						}
						else
							printf("invalid argument\n");
					}
					break;
				case 'r': // raw status
					if(checkCon())
					{
						xyz.queryStatusStart(true);
						while(xyz.isInProgress())
							doProcessWithSleep();
					}
					break;
				case 's': // status
					if(checkCon())
						printStatus();
					break;
				case 't': // test
					debugPrint(DBG_REPORT, "test!");
					if(checkCon())
					{
						runTimedTest(0.0f);
					}
					debugPrint(DBG_REPORT, "test finished");
					break;
				case 'u':
					if(checkCon())
					{
						printf("start unload filament\n");
						xyz.unloadFilamentStart();
						while(xyz.isInProgress())
							doProcessWithSleep(); // spin

						printf("unload filament succeeded\n");
					}
					break;
				case 'x':
					t = 10;
					if(i+1 < argc && !isKey(argv[i+1])) 
					{
						t = atoi(argv[i+1]);
						i++;
					}

					if(checkCon())
					{
						xyz.jogPrinterStart('x', t);
						while(xyz.isInProgress())
							doProcessWithSleep(); // spin
					}
					break;
				case 'y':
					t = 10;
					if(i+1 < argc && !isKey(argv[i+1]))
					{
						t = atoi(argv[i+1]);
						i++;
					}
					if(checkCon())
					{
						xyz.jogPrinterStart('y', t);
						while(xyz.isInProgress())
							doProcessWithSleep(); // spin
					}
					break;
				case 'z':
					t = 10;
					if(i+1 < argc && !isKey(argv[i+1]))
					{
						t = atoi(argv[i+1]);
						i++;
					}
					if(checkCon())
					{
						xyz.jogPrinterStart('z', t);
						while(xyz.isInProgress())
							doProcessWithSleep(); // spin
					}
					break;
				case 'i':
					{
						const char *path = "image.jif";
						if(i+1 < argc && !isKey(argv[i+1]))
						{
							path = argv[i+1];
							i++;
						}
						if(checkCon())
						{
							if(xyz.captureImage(path))
								printf("image captured\n");
							else
								printf("failed to capture image\n");
						}
					}
					break;
				default:
					printf("unknown argument '%s'\n", argv[i]);
					break;

				//****FixMe, hook these up!
				// setMachineNameStart("name")
				// sendDisconnectWifiStart()
				// setWifiStart(const char *ssid, const char *password, int channel)
				// restoreDefaultsStart()
				// setLanguageStart("en")
				// setEnergySavingStart(0-9)
				// setMachineLifeStart(int time_s)
				}
			}
		}
	}
	
	// disconnect just in case
	xyz.setStream(NULL);

	debugFinalize();

#if defined(_DEBUG) |  defined(DUMP_STATUS)
	printf("run took %0.4f seconds\n", tm.stopTimer());
	printf("\nhit any key to continue.\n");
	getch();
#endif

	return 0;
}
