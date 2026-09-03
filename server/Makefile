CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -Werror
TARGET := aesdsocket
SRC := aesdsocket.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
