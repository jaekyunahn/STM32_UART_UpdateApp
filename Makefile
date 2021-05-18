CC = gcc

INC =
CFLAGS = -g -O0 -Wall

OBJS = main.o function.o
SRCS = main.c function.c
LIBS = -lrt -pthread

TARGET = main

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $(OBJS)

dep :
	gccmakedep $(INC) $(SRCS)

clean :
	rm -rf $(OBJS) $(TARGET) core
