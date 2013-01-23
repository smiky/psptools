TARGET = keydump
OBJS = main.o imports.o
LIBS =
LIBDIR =
CFLAGS = -Os -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS =
ENCRYPT=1

BUILD_PRX = 1
PSP_EBOOT_TITLE += 550 keydump

EXTRA_TARGETS = EBOOT.PBP
PSP_FW_VERSION = 550

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
