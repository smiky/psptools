TARGET = 639kxploit
OBJS = main.o imports.o
LIBS =
LIBDIR =
CFLAGS = -Os -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LDFLAGS =
ENCRYPT=1

BUILD_PRX = 1
PSP_EBOOT_TITLE += 639 kxploit

EXTRA_TARGETS = EBOOT.PBP
PSP_FW_VERSION = 639

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
