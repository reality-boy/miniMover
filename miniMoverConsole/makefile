PROG = minimover
CC = g++
CPPFLAGS = -g -Wall -I ../
LDFLAGS = 
OBJS = minimover.o xyzv3.o serial_linux.o stream.o miniz.o debug.o aes.o timer.o
#	network.o 

$(PROG) : $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS)

minimover.o :
	$(CC) $(CPPFLAGS) -c minimover.cpp

xyzv3.o : 
	$(CC) $(CPPFLAGS) -c ../xyzv3.cpp

serial_linux.o : 
	$(CC) $(CPPFLAGS) -c ../serial_linux.cpp

stream.o : 
	$(CC) $(CPPFLAGS) -c ../stream.cpp

#network.o : 
#	$(CC) $(CPPFLAGS) -c ../network.cpp

miniz.o : 
	$(CC) $(CPPFLAGS) -c ../miniz.c

debug.o : 
	$(CC) $(CPPFLAGS) -c ../debug.cpp

aes.o : 
	$(CC) $(CPPFLAGS) -c ../aes.cpp

timer.o : 
	$(CC) $(CPPFLAGS) -c ../timer.cpp

clean:
	rm -f core $(PROG) $(OBJS)
