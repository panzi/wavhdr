CC=gcc
LD=$(CC)
TARGET=$(shell uname|tr '[A-Z]' '[a-z]')$(shell getconf LONG_BIT)
INCLUDE=
LIBDIRS=
CFLAGS+=-Wall -Werror -Wextra -Wno-type-limits -std=c11 -O2 -g $(INCLUDE) $(LIBDIRS) -D_FILE_OFFSET_BITS=64
APPNAME=wavhdr

ifeq ($(TARGET),win32)
	PLATFORM=windows
	CC=i686-w64-mingw32-gcc
	CFLAGS+=-DWINVER=0x500 -m32
	LDFLAGS+=-m32
	LIBS=$(WINDOWS_LIBS)
	APPNAME=wavhdr-win32.exe
else
ifeq ($(TARGET),win64)
	PLATFORM=windows
	CC=x86_64-w64-mingw32-gcc
	CFLAGS+=-DWINVER=0x500 -m64
	LDFLAGS+=-m64
	LIBS=$(WINDOWS_LIBS)
	APPNAME=wavhdr-win64.exe
else
ifeq ($(TARGET),linux32)
	CFLAGS+=-pedantic -m32
	LDFLAGS+=-m32
	APPNAME=wavhdr-linux32
else
ifeq ($(TARGET),linux64)
	CFLAGS+=-pedantic -m64
	LDFLAGS+=-m64
endif
endif
endif
endif

.PHONY: all clean

all: $(APPNAME)

$(APPNAME): wavhdr.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -v $(APPNAME)
