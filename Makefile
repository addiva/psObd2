# it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
# (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.

TARGET = pspobd2

OBJS  = main.o net.o elm.o obdtask.o ui.o gfx.o font.o font8x8.o camera.o \
        appconfig.o obd.o profile.o fuel.o cost.o accel.o json.o dtc_db.o

CFLAGS   = -O2 -G0 -Wall -Wextra -DPSP_BUILD
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS  = $(CFLAGS)

# SOLO le librerie non gia' aggiunte da build.mak/specs del PSPSDK.
# build.mak aggiunge: pspdebug pspdisplay pspge pspctrl pspnet pspnet_apctl
# le specs aggiungono in coda: libc psputility psprtc pspnet_inet
#                              pspnet_resolver pspsdk pspmodinfo pspuser
# Ripeterle qui spezza le tabelle di import ("stubs out of order") e
# -lpspkernel portava dentro import KERNEL: l'app non partiva su PSP reale.
LIBS = -lpspgu -lpspusbcam -lpspusb -lpspjpeg -lpspwlan

BUILD_PRX        = 1
PSP_FW_VERSION   = 660
EXTRA_TARGETS    = EBOOT.PBP
PSP_EBOOT_TITLE  = PSP OBD2 - massirito
PSP_EBOOT_ICON   = ICON0.PNG

# I target di test su PC non richiedono il toolchain PSP
ifeq ($(filter test preview clean-test,$(MAKECMDGOALS)),)
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
endif

# Controllo: nessun import da librerie kernel nel PRX finale
check-imports: $(TARGET).prx
	@if psp-strings $(TARGET).prx | grep -E "ForKernel|_driver" ; then \
		echo "ERRORE: il PRX importa librerie kernel"; exit 1; \
	else echo "OK: solo librerie user-mode"; fi

# ---- Test su PC (gcc) ----
HOST_LOGIC = obd.c fuel.c cost.c accel.c json.c profile.c appconfig.c dtc_db.c
HOST_ALL   = $(HOST_LOGIC) elm.c net.c obdtask.c ui.c gfx.c font.c font8x8.c camera.c

test: test_logic.c $(HOST_LOGIC)
	gcc -Wall -Wextra -O0 -g -o test_logic test_logic.c $(HOST_LOGIC) -lm
	./test_logic

# Anteprima PNG di tutte le schermate + test end-to-end con ELM327 simulato
preview: tools/ui_preview.c $(HOST_ALL)
	gcc -Wall -Wextra -O0 -g -I. -o ui_preview tools/ui_preview.c $(HOST_ALL) -lm

clean-test:
	rm -f test_logic ui_preview

.PHONY: test preview clean-test check-imports
