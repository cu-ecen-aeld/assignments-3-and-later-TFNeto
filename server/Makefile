CC ?= gcc

#Taret
TARGET = aesdsocket

# Flags
CFLAGS = -Wall -Werror -g
LDFLAGS =

#Sources
SRCS = aesdsocket.c

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
#Clean
clean:
	rm -rf *.o $(TARGET)
