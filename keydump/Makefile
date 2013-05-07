TARGET = KeyDumper
OBJS = main.o ./kirk_engine/crypto.o ./kirk_engine/kirk_engine.o imports.o 

INCDIR = ./include
CFLAGS = -Os -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

EXTRA_TARGETS = EBOOT.PBP
PSP_FW_VERSION = 620
PSP_LARGE_MEMORY = 1

BUILD_PRX = 1
PSP_EBOOT_TITLE += 620 keydump
#PRX_EXPORTS = exports.exp

#LIBDIR = ./lib
#LDFLAGS = -mno-crt0 -nostartfiles
#LIBS = -lpspsemaphore -lpsputilsforkernel

ENCRYPT = 1

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
