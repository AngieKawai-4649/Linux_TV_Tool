
#CXX	= g++
#CXXFLAGS = -O2 -Wall -pthread -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I/usr/include/PCSC
CC	= gcc
CFLAGS  = -O2 -Wall -pthread -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I/usr/include/PCSC -I./libnkf
OBJS = cvi_scan.o libnkf/libnkf.o libnkf/aribTOsjis.o
#LIBS	= -lsoftcas
LIBS	=
TARGET	= cvi_scan

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

depend:
	$(CC) -MM $(OBJS:.o=.cp) > Makefile.dep

-include Makefile.dep

