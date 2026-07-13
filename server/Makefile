CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra
LDFLAGS ?= -pthread -lrt
all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c $(LDFLAGS)

clean:
	rm -f aesdsocket *.o