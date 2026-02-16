#Compiler
CC = $(CROSS_COMPILE)gcc

#Taret
TARGET = aesdsocket

# Flags
CFLAGS = -Wall -Werror -g
LDFLAGS =

#Sources
SRCS = aesdsocket.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

#Clean
clean:
	rm -rf *.o $(TARGET)
