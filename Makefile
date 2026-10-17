TARGET = FutebolPSP
OBJS = src/main.o

INCDIR =
CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspgum -lpspgu -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspkernel -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Futebol PSP - Prototipo
PSP_EBOOT_ICON =

BUILD_PRX = 1
PSP_FW_VERSION = 500

include $(PSPSDK)/lib/build.mak
