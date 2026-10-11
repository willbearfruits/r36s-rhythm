# The R36S rhythm machine. The core (core/) is plain C11 that builds hosted and freestanding; the hosts around it:
#   make test       build/host/check: pass/fail checks (a group alone: build/host/check drum)
#   make wavs       build/out/*.wav to listen to (drums, the BASEMENT pattern, thump's kits)
#   make desktop    build/desktop/rhythm: the instrument in a window (SDL2, ALSA MIDI); make run starts it
#   make ref        reference renders from thump's C++ engine (~/Projects/thump), for the drum checks
#   make port       build/port/: the instrument as an ArkOS Port for the R36S (ARM64, built in Docker), and
#                   build/r36s-rhythm-port.zip, the release: unzip it into the ports folder (tools/port/Dockerfile
#                   makes the image)
#   make arm64      the core and the checks built for the R36S's ARM64 in Docker (Debian buster, glibc 2.28)

CC      ?= gcc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -Wall -Wextra -Wno-unused-parameter -Wshadow -fno-math-errno -Icore -MMD -MP
THUMP   ?= $(HOME)/Projects/thump

CORE    := $(wildcard core/*.c)
B       := build/host

.PHONY: all test ref arm64 clean wavs desktop run port
all: $(B)/check $(B)/render $(B)/rhythm-host $(B)/bench

$(B)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/check: $(CORE:%.c=$(B)/%.o) $(B)/test/check.o $(B)/test/plat_host.o
	$(CC) $(CFLAGS) $^ -o $@ -lm

$(B)/render: $(CORE:%.c=$(B)/%.o) $(B)/test/render.o $(B)/test/plat_host.o
	$(CC) $(CFLAGS) $^ -o $@ -lm

$(B)/rhythm-host: $(CORE:%.c=$(B)/%.o) $(B)/test/host.o $(B)/test/plat_host.o
	$(CC) $(CFLAGS) $^ -o $@ -lm

D       := build/desktop
desktop: $(D)/rhythm
$(D)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(shell sdl2-config --cflags) -c $< -o $@
$(D)/rhythm: $(CORE:%.c=$(D)/%.o) $(D)/desktop/main.o $(D)/desktop/midi_alsa.o $(D)/desktop/workers.o $(D)/test/plat_host.o
	$(CC) $(CFLAGS) $^ -o $@ $(shell sdl2-config --libs) -lasound -lpthread -lm
run: $(D)/rhythm build/card/BUNDLED/BREAKS/DUSTY_94.WAV
	$(D)/rhythm
build/card/BUNDLED/BREAKS/DUSTY_94.WAV: $(B)/render
	$(B)/render break

$(B)/bench: $(CORE:%.c=$(B)/%.o) $(B)/test/bench.o $(B)/test/plat_host.o $(B)/desktop/workers.o
	$(CC) $(CFLAGS) $^ -o $@ -lm -lpthread

test: $(B)/check
	$(B)/check

wavs: $(B)/render
	$(B)/render

ref:
	@mkdir -p build/ref
	g++ -std=c++17 -O2 -I$(THUMP)/src/dsp tools/thump_ref.cpp $(THUMP)/src/dsp/Presets.cpp $(THUMP)/src/dsp/Render.cpp \
	    $(THUMP)/src/dsp/Params.cpp $(THUMP)/src/dsp/Slot.cpp $(THUMP)/src/dsp/Kit.cpp -o build/ref/thump-ref
	build/ref/thump-ref build/ref

port: build/card/BUNDLED/BREAKS/DUSTY_94.WAV
	docker run --rm --platform linux/arm64 --user $$(id -u):$$(id -g) -v $(CURDIR):/src -w /src r36s-build:buster \
	    make D=build/port-obj build/port-obj/rhythm CFLAGS="-O2 -g -mcpu=cortex-a35"
	rm -rf build/port && mkdir -p build/port/rhythm/card/BUNDLED/BREAKS
	cp build/port-obj/rhythm build/port/rhythm/
	cp build/card/BUNDLED/BREAKS/DUSTY_94.WAV build/port/rhythm/card/BUNDLED/BREAKS/
	cp tools/port/Rhythm.sh tools/port/INSTALL.txt build/port/
	cp tools/port/README.txt LICENSE NOTICE.md build/port/rhythm/
	cp -r LICENSES build/port/rhythm/
	cd build/port && rm -f ../r36s-rhythm-port.zip && zip -qr9 ../r36s-rhythm-port.zip Rhythm.sh INSTALL.txt rhythm

arm64:
	docker run --rm --platform linux/arm64 -v $(CURDIR):/src -w /src r36s-build:buster \
	    make B=build/arm64 build/arm64/check CFLAGS="-O2 -g"

clean:
	rm -rf build/host build/arm64

-include $(shell find build -name '*.d' 2>/dev/null)
