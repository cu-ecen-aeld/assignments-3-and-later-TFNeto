#Compiler
CC := $(CROSS_COMPILE)gcc

#Taret
TARGET := writer

# Flags
CFLAGS := -Wall -Werror -g
LDFLAGS :=

#Sources
SRCS := writer.c

#Objects
OBJS := $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

#Clean
clean:
	rm -rf *.o $(TARGET)

