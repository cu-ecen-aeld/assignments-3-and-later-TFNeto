CROSS_COMPILE ?= 
CC ?= $(CROSS_COMPILE)gcc

#Taret
TARGET = aesdsocket

# Flags
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?= -lpthread -lrt

#Sources
SRCS = aesdsocket.c

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
#Clean
clean:
	rm -rf *.o $(TARGET)
