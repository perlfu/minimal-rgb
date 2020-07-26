CFLAGS=-Wall -Werror
LDFLAGS=-lhidapi-libusb

wraith: wraith.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)


