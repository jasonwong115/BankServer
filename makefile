CC=gcc
PROGRAMS=appserver appservercoarse

all: $(PROGRAMS)

#executables
appserver: appserver.o Bank.o
	$(CC) -pthread -g -o appserver appserver.o Bank.o
appservercoarse:appservercoarse.o Bank.o
	$(CC) -pthread -g -o appservercoarse appservercoarse.o Bank.o

#create object files
appserver.o: appserver.c
	$(CC) -g -c appserver.c
appservercoarse.o: appservercoarse.c
	$(CC) -g -c appservercoarse.c
Bank.o: Bank.c
	$(CC) -g -c Bank.c

#cleanup object files
clean:
	rm -f $(PROGRAMS) *.o

