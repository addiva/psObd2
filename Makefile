# it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
# (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.

TARGET = pspobd2
TITLE  = "PSP OBD2 - massirito"

# Sorgenti PSP (include moduli PSP-specifici)
OBJS  = main.o net.o elm.o ui.o camera.o appconfig.o \
        obd.o profile.o fuel.o cost.o accel.o json.o

# Flag per il codice PSP
CFLAGS  = -O2 -G0 -Wall -Wextra -DPSP_BUILD
CFLAGS += -I$(PSPDEV)/psp/sdk/include
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

# Librerie PSP
LIBS = -lpspgu -lpspge -lpspdisplay -lpspctrl \
       -lpspnet -lpspnet_inet -lpspnet_apctl \
       -lpspusb -lpspusbcam \
       -lpsputility -lpsprtc -lpspiofilemgr \
       -lpspsdk -lpspkernel \
       -lm

# PARAM.SFO / EBOOT.PBP
BUILD_PRX        = 1
PSP_FW_VERSION   = 660
EXTRA_TARGETS    = EBOOT.PBP
PSP_EBOOT_TITLE  = $(TITLE)
PSP_EBOOT_ICON   = ICON0.PNG

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# ---- Test host (gcc) ----
TEST_SRCS = test_logic.c obd.c fuel.c cost.c accel.c json.c profile.c appconfig.c
TEST_BIN  = test_logic

test: $(TEST_SRCS)
	gcc -Wall -Wextra -O0 -g \
	    -o $(TEST_BIN) $(TEST_SRCS) \
	    -lm
	./$(TEST_BIN)

clean-test:
	rm -f $(TEST_BIN)

.PHONY: test clean-test
