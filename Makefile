###############################################################################
# cweather makefile for linux
###############################################################################

all: cweather memdump readdump

CC       ?= gcc
CFLAGS   ?= -Wall -g
ECPG     ?= ecpg

# Objects required by programs doing device I/O
DEVIO_OBJ = lib/hidapi/linux-0.7/hid-libusb.o src/deviceio.o

# C code generated from ECPG sources
PGC_SRC   = pgout.c

# Objects required by programs doing database I/O
DBIO_OBJ  = pgout.o

# Objects for cweather
CW_OBJS   = src/main.o src/deviceconfig.o src/conout.o src/debug.o src/history.o src/fileout.o $(DEVIO_OBJ) $(DBIO_OBJ)

# Objects for memdump
MD_OBJS   = src/memdump.o src/debug.o $(DEVIO_OBJ)

# Objects for readdump
RD_OBJS   = src/readdump.o src/fileio.o src/conout.o src/deviceconfig.o src/history.o

# All objects that don't require memdump stuff
ALL_OBJS      = src/main.o src/deviceconfig.o src/conout.o src/debug.o src/history.o src/fileout.o

# USB library stuff
USB_LIBS      = `pkg-config libusb-1.0 --libs`
USB_INCLUDES  = -Ilib/hidapi/linux-0.7 `pkg-config libusb-1.0 --cflags`

# ECPG (database) library stuff
ECPG_LIBS     = -lecpg
ECPG_INCLUDES = -I/usr/include/postgresql

cweather : $(CW_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(USB_LIBS) $(ECPG_LIBS) -o cweather

memdump: $(MD_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(USB_LIBS) -o memdump

readdump: $(RD_OBJS) 
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o readdump

$(DEVIO_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c $(USB_INCLUDES) $< -o $@

$(DBIO_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c $(ECPG_INCLUDES) -Isrc $< -o $@

# Run ECPG sources through ECPG to get the resulting C code.
$(PGC_SRC): %.c: src/%.pgc
	$(ECPG) -o $@ $<

$(ALL_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ALL_OBJS) $(DEVIO_OBJ) $(DBIO_OBJ) $(PGC_SRC) cweather memdump readdump

.PHONY: clean

