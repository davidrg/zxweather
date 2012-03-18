###############################################################################
# cweather makefile for linux
###############################################################################

all: cweather

CC       ?= gcc
CFLAGS   ?= -Wall -g

OBJS     = main.o deviceconfig.o deviceio.o dc_conout.o debug.o hidapi/linux-0.7/hid-libusb.o
LIBS      = `pkg-config libusb-1.0 --libs`
INCLUDES ?= -Ihidapi/linux-0.7 `pkg-config libusb-1.0 --cflags`

cweather : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) -o cweather

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS) cweather

.PHONY: clean

