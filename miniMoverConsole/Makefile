PROG = minimover
CC = g++
CPPFLAGS = -g -Wall -I../miniMoverLib/ -D _DEBUG
LDFLAGS = 
OBJS = minimover.o xyzv3.o XYZPrinterList.o serial_linux.o stream.o miniz.o debug.o aes.o timer.o socket_linux.o 

$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS)

minimover.o: minimover.cpp
	$(CC) $(CPPFLAGS) -c minimover.cpp

xyzv3.o: ../miniMoverLib/xyzv3.cpp ../miniMoverLib/xyzv3.h
	$(CC) $(CPPFLAGS) -c ../miniMoverLib/xyzv3.cpp

XYZPrinterList.o: ../miniMoverLib/XYZPrinterList.cpp ../miniMoverLib/XYZPrinterList.h
	$(CC) $(CPPFLAGS) -c ../miniMoverLib/XYZPrinterList.cpp

serial_linux.o: ../miniMoverLib/serial_linux.cpp ../miniMoverLib/serial.h
	$(CC) $(CPPFLAGS) -c ../miniMoverLib/serial_linux.cpp

stream.o: ../miniMoverLib/stream.cpp ../miniMoverLib/stream.h
	$(CC) $(CPPFLAGS) -c ../miniMoverLib/stream.cpp

socket_linux.o: ../miniMoverLib/socket_linux.cpp ../miniMoverLib/socket.h
	$(CC) $(CPPFLAGS) -c ../miniMoverLib/socket_linux.cpp

miniz.o: ../miniMoverLib/miniz.c ../miniMoverLib/miniz.h
	$(CC) $(CPPFLAGS) -c ../miniMoverLib/miniz.c

debug.o: ../miniMoverLib/debug.cpp ../miniMoverLib/debug.h
	$(CC) $(CPPFLAGS) -c ../miniMoverLib/debug.cpp

aes.o: ../miniMoverLib/aes.cpp ../miniMoverLib/aes.h
	$(CC) $(CPPFLAGS) -c ../miniMoverLib/aes.cpp

timer.o: ../miniMoverLib/timer.cpp ../miniMoverLib/timer.h
	$(CC) $(CPPFLAGS) -c ../miniMoverLib/timer.cpp

clean:
	rm -f core $(PROG) $(OBJS)
