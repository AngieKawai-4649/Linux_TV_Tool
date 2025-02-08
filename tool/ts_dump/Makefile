
CC	= gcc
CFLAGS  = -O2 -Wall -pthread -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
OBJS = ts_dump.o
TARGET	= ts_dump

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS)

depend:
	$(CC) -MM $(OBJS:.o=.c) > Makefile.dep

-include Makefile.dep

