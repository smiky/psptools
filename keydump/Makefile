TARGET = KeyDumper
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE += 620 keydump
OBJS = main.o imports.o

#INCDIR = ../include
CFLAGS = -Os -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

PSP_FW_VERSION = 620

BUILD_PRX = 1
#PRX_EXPORTS = exports.exp

LIBDIR = ./
#LDFLAGS = -mno-crt0 -nostartfiles
LIBS = -lpspmodulemgr_kernel

ENCRYPT = 1

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
