PROG = minimover
CC = g++
CPPFLAGS = -g -Wall -I../ -D _DEBUG
LDFLAGS = 
OBJS = minimover.o xyzv3.o serial_linux.o stream.o miniz.o debug.o aes.o timer.o network_linux.o 

$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS)

minimover.o:
	$(CC) $(CPPFLAGS) -c minimover.cpp

xyzv3.o: ../xyzv3.cpp ../xyzv3.h
	$(CC) $(CPPFLAGS) -c ../xyzv3.cpp

serial_linux.o: ../serial_linux.cpp ../serial.h
	$(CC) $(CPPFLAGS) -c ../serial_linux.cpp

stream.o: ../stream.cpp ../stream.h
	$(CC) $(CPPFLAGS) -c ../stream.cpp

network_linux.o: ../network_linux.cpp ../network.h
	$(CC) $(CPPFLAGS) -c ../network_linux.cpp

miniz.o: ../miniz.c ../miniz.h
	$(CC) $(CPPFLAGS) -c ../miniz.c

debug.o: ../debug.cpp ../debug.h
	$(CC) $(CPPFLAGS) -c ../debug.cpp

aes.o: ../aes.cpp ../aes.h
	$(CC) $(CPPFLAGS) -c ../aes.cpp

timer.o: ../timer.cpp ../timer.h
	$(CC) $(CPPFLAGS) -c ../timer.cpp

clean:
	rm -f core $(PROG) $(OBJS)
