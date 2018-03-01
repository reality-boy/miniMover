#include <SDKDDKVer.h>
#include <Windows.h>
#include <stdio.h>
#include <conio.h> 
#include <time.h>

#include "timer.h"
#include "serial.h"
#include "xyzv3.h"

#pragma warning(disable:4996) // live on the edge!

XYZV3 xyz;
int port = -1;

void updateStatus(float pct)
{
	// notify the user that we are still uploading
	static int printCount = 0;
	if(printCount % 4 == 0)
		printf(".");
	printCount++;
}

void postHelp()
{
	printf("miniMover v0.4\n");
	printf("usage: miniMover <args>\n");
	printf("  -? - print help message\n");
	printf("  -a+ - enable auto level\n");
	printf("  -a- - disable auto level\n");
	printf("  -b+ - enable buzzer\n");
	printf("  -b- - disable buzzer\n");
	printf("  -cl - clean nozzel\n");
	printf("  -ca - calibrate bed\n");
	printf("  -c file - convert file\n");
	printf("  -f file - upload firmware, experimental!\n");
	printf("  -h - home printer\n");
	printf("  -l - load fillament\n");
	printf("  -o num - increment/decrement z offset by num\n");
	printf("  -po num - set serial port number, -1 auto detects port\n");
	printf("  -p file - print file\n");
	printf("  -r - print raw status\n");
	printf("  -s - print status\n");
	printf("  -u - unload fillament\n");
	printf("  -x num - jog x axis by num, or 10 if num not provided\n");
	printf("  -y num - jog y axis by num, or 10 if num not provided\n");
	printf("  -z num - jog z axis by num, or 10 if num not provided\n");
	printf("  file - print file if .gcode, otherwise convert to gcode if .3w\n");
}

bool checkCon()
{
	if(xyz.connect(port))
		return true;

	if(port >= 0)
		printf("printer not found on port: %d\n", port);
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
	if(xyz.updateStatus())
	{
		const XYZPrinterState *st = xyz.getPrinterState();
		const XYZPrinterInfo *inf = xyz.getPrinterInfo();
		if(st->isValid)
		{
			printf("Bed temperature: %d C\n", st->bedTemp_C);

			printf("Bed calibration\n");
			printf(" %d, %d, %d\n", st->calib[0], st->calib[1], st->calib[2]);
			printf(" %d, %d, %d\n", st->calib[3], st->calib[4], st->calib[5]);
			printf(" %d, %d, %d\n", st->calib[6], st->calib[7], st->calib[8]);

			if(st->printPercentComplete == 0 && st->printElapsedTime_m == 0 && st->printTimeLeft_m == 0)
				printf("No job running\n");
			else
				printf("Printer status: %d %% %d m %d m\n", st->printPercentComplete, st->printElapsedTime_m, st->printTimeLeft_m);

			printf("Error: %d\n", st->errorStatus);

			printf("Filament length: %0.3f m\n", st->fillimantRemaining_mm / 1000.0f);

			printf("PLA filament: %d\n", st->isFillamentPLA);

			printf("Serial number: %s\n", st->machineSerialNum);

			printf("Status: %s\n", st->printerStatusStr);

			//printf("material type: %d\n", st->materialType);

			printf("language: %s\n", st->lang);

			printf("printer name: %s\n", st->machineName);

			printf("Package size: %d bytes\n", st->packetSize);

			printf("Auto level %s\n", (st->autoLevelEnabled) ? "enabled" : "disabled");

			printf("Z-Offset %d\n", st->zOffset);

			printf("Model number: %s\n", st->machineModelNumber);

			if(inf)
			{
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

			printf("buzzer: %s\n", (st->buzzerEnabled) ? "enabled" : "disabled");

			printf("Extruder temp: %d C, target temp: %d C\n", st->extruderActualTemp_C, st->extruderTargetTemp_C);

			printf("Firmware version: %s\n", st->firmwareVersion);

			printf("Filament SN: %s\n", st->filamentSerialNumber);

			printf("Machine power on time: %d min\n", st->printerLifetimePowerOnTime_min);
			printf("Extruder power on time: %d min\n", st->extruderLifetimePowerOnTime_min);
			printf("Last power on time: %d min\n", st->printerLastPowerOnTime_min);

			printf("nozel: %0.2f mm sn: %s\n", st->nozelDiameter_mm, st->nozelSerialNumber);
		}
		// else do nothing

		return true;
	}

	return false;
}

bool monitorPrintJob()
{
	if(xyz.updateStatus())
	{
		const XYZPrinterState *st = xyz.getPrinterState();
		const XYZPrinterInfo *inf = xyz.getPrinterInfo();
		if(st->isValid)
		{
			printf("S: %s, temp: %d C / %d C", 
				st->printerStatusStr,
				st->extruderActualTemp_C, st->extruderTargetTemp_C);
			if(st->bedTemp_C > 30)
				printf(" - %d C", st->bedTemp_C);

			if(st->printPercentComplete != 0 || st->printElapsedTime_m != 0 || st->printTimeLeft_m != 0)
				printf(" Job: %d %% %d m %d m", st->printPercentComplete, st->printElapsedTime_m, st->printTimeLeft_m);

			if(st->errorStatus)
				printf(" Error: %d", st->errorStatus);

			printf("\n");
		}
		// else just wait till next cycle

		// if not PRINT_NONE
		return ( st->printerStatus != PRINT_ENDING_PROCESS_DONE &&
				 st->printerStatus != PRINT_NONE);
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
					char c = getch();

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

				Sleep(10); // don't spin too fast
			}

			return true;
		}
	}

	return false;
}

int main(int argc, char **argv)
{
	if(argc <= 1)
	{
		postHelp();
		return 0;
	}

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
			switch(argv[i][1])
			{
			case '?':
				postHelp();
				break;
			case 'a':
				if(checkCon())
					xyz.enableAutoLevel(argv[i][2] != '-');
				break;
			case 'b':
				if(checkCon())
					xyz.enableBuzzer(argv[i][2] != '-');
				break;
			case 'c':
				if(argv[i][2] == 'l') // clean
				{
					if(checkCon())
					{
						printf("starting clean nozzel\n");
						if(xyz.cleanNozzleStart())
						{
							printf("clean nozzle with a wire and press enter when finished\n");
							getch();

							if(xyz.cleanNozzleFinish())
							{
								printf("clean nozel succeeded\n");
							}
							else
								printf("clean nozel failed\n");
						}
						else
							printf("clean nozel failed\n");
					}
				}
				else if(argv[i][2] == 'a') // calibrate
				{
					if(checkCon())
					{
						printf("starting calibration\n");
						if(xyz.calibrateBedStart())
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
					if(xyz.homePrinter())
						printf("home printer succeeded\n");
					else
						printf("home printer failed\n");
				}
				break;
			case 'l':
				if(checkCon())
				{
					printf("starting load fillament\n");
					if(xyz.loadFillamentStart())
					{
						printf("wait for fillament to come out of nozel then hit enter\n");
						getch();

						if(xyz.loadFillamentFinish())
						{
							printf("load fillament succeeded\n");
						}
						else
							printf("load fillament failed\n");
					}
					else
						printf("load fillament failed\n");
				}
				break;
			case 'o':
				if(i+1 < argc && !isKey(argv[i+1]))
				{
					int t = atoi(argv[i+1]);
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
						port = atoi(argv[i+1]);
						i++;
					} else 
						printf("port needs a number\n");
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
			case 'r': // status
				if(checkCon())
					xyz.printRawStatus();
				break;
			case 's': // status
				if(checkCon())
					printStatus();
				break;
			case 'u':
				if(checkCon())
				{
					printf("start unload fillament\n");
					if(xyz.unloadFillament())
						printf("unload fillament succeeded\n");
					else
						printf("unload fillament failed\n");
				}
				break;
			case 'x':
				if(i+1 < argc && !isKey(argv[i+1])) 
				{
					int t = atoi(argv[i+1]);
					if(checkCon())
					{
						if(!xyz.jogPrinter('x', t))
							printf("jog printer failed\n");
					}
					i++;
				}
				else
				{
					if(checkCon())
					{
						if(!xyz.jogPrinter('x', 10))
							printf("jog printer failed\n");
					}
				}
				break;
			case 'y':
				if(i+1 < argc && !isKey(argv[i+1]))
				{
					int t = atoi(argv[i+1]);
					if(checkCon())
					{
						if(!xyz.jogPrinter('y', t))
							printf("jog printer failed\n");
					}
					i++;
				}
				else
				{
					if(checkCon())
					{
						if(!xyz.jogPrinter('y', 10))
							printf("jog printer failed\n");
					}
				}
				break;
			case 'z':
				if(i+1 < argc && !isKey(argv[i+1]))
				{
					int t = atoi(argv[i+1]);
					if(checkCon())
					{
						if(!xyz.jogPrinter('z', t))
							printf("jog printer failed\n");
					}
					i++;
				}
				else
				{
					if(checkCon())
					{
						if(!xyz.jogPrinter('z', 10))
							printf("jog printer failed\n");
					}
				}
				break;
			default:
				printf("unknown argument %s\n", argv[i]);
				break;
			}
		}
	}
	
	// disconnect just in case
	xyz.disconnect();

#ifdef _DEBUG
	printf("hit any key to continue.\n");
	getch();
#endif

	return 0;
}
