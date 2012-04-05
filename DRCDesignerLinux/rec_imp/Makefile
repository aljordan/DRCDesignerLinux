### RtAudio tests Makefile - for various flavors of unix

PROGRAMS = rec_imp
RM = /bin/rm
INCLUDE = /usr/include
vpath %.o $(OBJECT_PATH)

OBJECTS	=	RtAudio.o fftsg_h.o

CC       = g++
DEFS     = -DUseDouble
AUDIO_DEFS    += -D__LINUX_OSS__ -D__LINUX_ALSA__
CFLAGS   = -O
CFLAGS  += -Wall -I$(INCLUDE)
#CFLAGS  += -g
AUDIO_LIBRARY  = -lasound -lpthread
LIBRARY = 

# Uncomment if you use JACK
AUDIO_DEFS    +=  -D__LINUX_JACK__
AUDIO_LIBRARY += `pkg-config --cflags --libs jack`


all : $(PROGRAMS)

RtAudio.o : RtAudio.cpp
	$(CC) $(CFLAGS) $(DEFS) $(AUDIO_DEFS) -c $(<) -o RtAudio.o

fftsg_h.o : fftsg_h.cpp
	$(CC) $(CFLAGS) $(DEFS) -c $(<) -o fftsg_h.o

rec_imp : rec_imp.cpp $(OBJECTS)
	$(CC) $(CFLAGS) $(DEFS) $(AUDIO_DEFS) -o rec_imp rec_imp.cpp $(OBJECTS) $(AUDIO_LIBRARY)

clean : 
	-rm *.o
	-rm $(PROGRAMS)
	-rm -f *~

strip : 
	strip $(PROGRAMS)
