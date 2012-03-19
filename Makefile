###############################################################################
# cweather makefile for linux
###############################################################################

all: cweather memdump readdump

CC       ?= gcc
CFLAGS   ?= -Wall -g

# Objects required by programs doing device I/O
DEVIO_OBJ = lib/hidapi/linux-0.7/hid-libusb.o src/deviceio.o

# Objects for cweather
CW_OBJS   = src/main.o src/deviceconfig.o src/conout.o src/debug.o src/history.o $(DEVIO_OBJ)

# Objects for memdump
MD_OBJS   = src/memdump.o src/debug.o $(DEVIO_OBJ)

# Objects for readdump
RD_OBJS   = src/readdump.o src/fileio.o src/conout.o src/deviceconfig.o src/history.o

# All objects that don't require memdump stuff
ALL_OBJS      = src/main.o src/deviceconfig.o src/conout.o src/debug.o src/history.o

LIBS      = `pkg-config libusb-1.0 --libs`
INCLUDES ?= -Ilib/hidapi/linux-0.7 `pkg-config libusb-1.0 --cflags`

cweather : $(CW_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) -o cweather

memdump: $(MD_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) -o memdump

readdump: $(RD_OBJS) 
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o readdump

$(DEVIO_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDES) $< -o $@

$(ALL_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ALL_OBJS) $(DEVIO_OBJ) cweather memdump readdump

.PHONY: clean

