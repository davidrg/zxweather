###############################################################################
# cweather makefile for linux
###############################################################################

all: cweather

CC       ?= gcc
CFLAGS   ?= -Wall -g

HIDAPIOBJ = lib/hidapi/linux-0.7/hid-libusb.o
OBJS      = src/main.o src/deviceconfig.o src/deviceio.o src/dc_conout.o src/debug.o $(HIDAPIOBJ)
LIBS      = `pkg-config libusb-1.0 --libs`
INCLUDES ?= -Ilib/hidapi/linux-0.7 `pkg-config libusb-1.0 --cflags`

cweather : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) -o cweather

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS) cweather

.PHONY: clean

