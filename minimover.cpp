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

void postHelp()
{
	printf("usage: miniMover <args>\n");
	printf("  -? - print help message\n");
	printf("  -a+ - enable auto level\n");
	printf("  -a- - disable auto level\n");
	printf("  -b+ - enable buzzer\n");
	printf("  -b- - disable buzzer\n");
	printf("  -cl - clean nozzel\n");
	printf("  -ca - calibrate bed\n");
	printf("  -c file - convert file\n");
	printf("  -h - home printer\n");
	printf("  -l - load fillament\n");
	printf("  -o num - increment/decrement z offset by num\n");
	printf("  -po num - set serial port number, -1 auto detects port\n");
	printf("  -p file - print file\n");
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

bool isGcodeFile(const char *path)
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

bool is3wFile(const char *path)
{
	if(path)
	{
		const char *p = strrchr(path, '.');
		if(p)
			return 0 == strcmp(p, ".3w");
	}

	return false;
}

bool convertFile(const char *path)
{
	if(path)
	{
		if(isGcodeFile(path))
		{
			// try to connect to get the machine id,
			// but convert anyway if we don't connect
			checkCon(); 
			return xyz.encryptFile(path);
		}

		if(is3wFile(path))
			return xyz.decryptFile(path);
	}

	return false;
}

// accepts a 3w file formatted for this printer
bool printFile(const char *path)
{
	if( path &&
		checkCon())
	{
		if(isGcodeFile(path))
		{
			const char *temp = "temp.3w";
			if(xyz.encryptFile(path, temp))
			{
				// send to printer
				if(xyz.printFile(temp))
				{
					// loop till print finished
					while(xyz.monitorPrintJob())
						Sleep(5000);

					remove(temp);

					return true;
				}
			}
		}
		else if(is3wFile(path))
		{
			// send to printer
			if(xyz.printFile(path))
			{
				// loop till print finished
				while(xyz.monitorPrintJob())
					Sleep(5000);

				return true;
			}
		}
	}

	return false;
}

bool isKey(const char *str)
{
	return (str && str[0] == '-' && !isdigit(str[1]));
}

void calibPress()
{
	printf("lower detector and hit enter to continue...\n");
	getch();
}

void calibRelease()
{
	printf("raise detector and hit enter to continue...\n");
	getch();
}

void loadDone()
{
	printf("wait for fillament to come out of nozel then hit enter\n");
	getch();
}

void cleanDone()
{
	printf("clean nozzle with a wire and press enter when finished\n");
	getch();
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
			if(isGcodeFile(argv[i]))
			{
				printf("starting print file\n");
				if(printFile(argv[i]))
					printf("print file succeeded\n");
				else
					printf("print file failed to print %s\n", argv[i+1]);
			}
			else if(is3wFile(argv[i]))
			{
				printf("starting convert file\n");
				if(convertFile(argv[i]))
					printf("convert file succeeded\n");
				else
					printf("convert file failed to convert %s\n", argv[i+1]);
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
						if(xyz.cleanNozzle(cleanDone))
							printf("clean nozel succeeded\n");
						else
							printf("clean nozel failed\n");
					}
				}
				else if(argv[i][2] == 'a') // calibrate
				{
					if(checkCon())
					{
						printf("starting calibration\n");
						if(xyz.calibrateBed(calibPress, calibRelease))
							printf("calibration completed succesfully\n");
						else
							printf("calibration failed\n");
					}
				}
				else // convert
				{
					if(i+1 < argc && !isKey(argv[i+1])) 
					{
						if(checkCon())
						{
							printf("starting convert file\n");
							if(convertFile(argv[i+1]))
								printf("convert file succeeded\n");
							else
								printf("convert file failed to convert %s\n", argv[i+1]);
						}
						i++;
					}
					else
						printf("invalid argument\n");
				}
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
					if(xyz.loadFillament(loadDone))
						printf("load fillament succeeded\n");
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
							if(printFile(argv[i+1]))
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
			case 's': // status
				if(checkCon())
					xyz.printStatus();
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

	return 0;
}

