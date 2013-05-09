TARGET = CipherBridge
OBJS = main.o memlmd_reverse.o mesgled_reverse.o mesg_led_02g.o

INCDIR = ../include
CFLAGS = -O2 -G0 -Wall -fno-pic
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

PSP_FW_VERSION = 271

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC=1
USE_KERNEL_LIBS=1

LIBDIR = ../lib
LDFLAGS = -mno-crt0 -nostartfiles
LIBS = -lpspsemaphore -lpspsystemctrl_kernel -lpsputilsforkernel

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
