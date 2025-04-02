PULSE_HEADERS ?= "../pulseaudio-v15.99.1/build"
PULSE_MODULE_HEADERS ?= "../pulseaudio-v15.99.1/src/modules"
PULSE_PULSE_HEADERS1 ?= "../pulseaudio-v15.99.1/src/pulse"
PULSE_PULSE_HEADERS2 ?= "../pulseaudio-v15.99.1/src"
DBUS_HEADERS ?= "/usr/include/dbus-1.0"
DBUS_ARCH_HEADERS ?= "/usr/lib/x86_64-linux-gnu/dbus-1.0/include"

PULSE_DEFINES ?= __INCLUDED_FROM_PULSE_AUDIO
LADSPA_PATHS_DEFINE ?= LADSPA_PATH="$(libdir)/ladspa:/usr/local/lib/ladspa:/usr/lib/ladspa:/usr/local/lib64/ladspa:/usr/lib64/ladspa"
LADSPA_MODULE_DEFINE ?= PA_MODULE_NAME=module_load_module_per_hardware_sink

INSTALL_LOCATION = "/usr/lib"

export MYCFLAGS := -fPIC $(CFLAGS) -D$(PULSE_DEFINES) -D$(LADSPA_PATHS_DEFINE) -D$(LADSPA_MODULE_DEFINE) -I $(DBUS_HEADERS) -I $(DBUS_ARCH_HEADERS) -I $(PULSE_HEADERS) -I $(PULSE_MODULE_HEADERS) -I $(PULSE_PULSE_HEADERS1) -I $(PULSE_PULSE_HEADERS2) -I $(PULSE_HEADERS)/src -O2

module-load-module-per-hardware-sink.so : module-load-module-per-hardware-sink.o
		$(CC) $(MYCFLAGS) -shared -o module-load-module-per-hardware-sink.so module-load-module-per-hardware-sink.o

module-load-module-per-hardware-sink.o : module-load-module-per-hardware-sink.c 
		$(CC) $(MYCFLAGS) -c module-load-module-per-hardware-sink.c

install:
		install module-load-module-per-hardware-sink.so $(INSTALL_LOCATION)

clean :
		rm module-load-module-per-hardware-sink.so module-load-module-per-hardware-sink.o
