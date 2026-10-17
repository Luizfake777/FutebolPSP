
TARGET = FutebolPSP
OBJS = src/main.o

INCDIR =
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PSP_FW_VERSION = 660

LIBS = -lpspgum -lpspgu -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = FutebolPSP

PSPSDK := $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak
