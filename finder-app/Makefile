CROSS_COMPILE ?=
#Compiler
CC ?= $(CROSS_COMPILE)gcc


#Taret
TARGET = writer

# Flags
CFLAGS = -Wall -Werror -g
LDFLAGS =

#Sources
SRCS = writer.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

#Clean
clean:
	rm -rf *.o $(TARGET)

