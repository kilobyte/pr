#CC=gcc -Wall -I include/
#CC=/cygdrive/c/MinGW/bin/gcc -O2 -Wall -I include/
CC=i586-mingw32msvc-gcc -O2 -Wall
#WINDRES=windres
WINDRES=i586-mingw32msvc-windres
LDFLAGS=-Xlinker --subsystem -Xlinker windows

default: all

all: pr.exe pod.exe prh.exe

clean:
	rm -f *.o pr.exe *.coff

pr.exe: pr.o pr.coff epson.o print.o
	${CC} ${LDFLAGS} -o pr.exe pr.o pr.coff epson.o print.o -lgdi32 -lcomdlg32

prh.exe: prh.o pr.coff epson.o print.o
	${CC} ${LDFLAGS} -o prh.exe prh.o pr.coff epson.o print.o -lgdi32 -lcomdlg32

pr.o: pr.c charsets.h
	${CC} -c pr.c
	
prh.o: pr.c charsets.h
	${CC} -DPRH -c pr.c -o prh.o

pr.coff: pr.rc icon.ico
	${WINDRES} pr.rc pr.coff


pod.exe: pod.o podtest.o pr.coff
	${CC} ${LDFLAGS} -o pod.exe pod.o podtest.o pr.coff -lgdi32 -lcomdlg32

pod.o: pod.c charsets.h
	${CC} -c pod.c

podtest.o: podtest.c
	${CC} -c podtest.c

epson.o: epson.c charsets.h printdoc.h
	${CC} -c epson.c

print.o: print.c printdoc.h
	${CC} -c print.c
