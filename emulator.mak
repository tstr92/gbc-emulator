# Makefile for GBC-Emulator

TARGET=emulator

COMPILER = /c/msys64/mingw64/bin
CC = $(COMPILER)/gcc
OBJDUMP = $(COMPILER)/objdump
BIN	= $(COMPILER)/objcopy

SDL_PATH=/c/libs/SDL2-2.32.0/x86_64-w64-mingw32
SDL_INC=$(SDL_PATH)/include/SDL2
SDL_LIB=$(SDL_PATH)/bin

SDL_TTF_PATH=/c/libs/SDL2_ttf-2.24.0/x86_64-w64-mingw32
SDL_TTF_INC=$(SDL_TTF_PATH)/include/SDL2
SDL_TTF_LIB=$(SDL_TTF_PATH)/bin

OUTDIR = .release

DEBUG?=0
PER_PIXEL_DRAW?=0

VPATH = src

ifeq ($(MAKELEVEL),0)
	dummy := $(shell echo -e "Make Emulator")
	dummy := $(shell mkdir -p $(OUTDIR))
endif

SRC = \
		main.c \
		emulator.c \
		cpu.c \
		bus.c \
		joypad.c \
		timer.c \
		serial.c \
		apu.c \
		ppu.c \
		trace.c \
		ppu_debug.c

OBJS = $(addprefix $(OUTDIR)/,$(SRC:.c=.o))

CFLAGS = \
		-I$(SDL_INC) \
		-I$(SDL_TTF_INC) \
		-DDEBUG=$(DEBUG) \
		-DUSE_0xE000_AS_PUTC_DEVICE=0 \
		-DPER_PIXEL_DRAW=$(PER_PIXEL_DRAW) \
		-ffunction-sections \
		-fdata-sections \
		-g \
		-O0

#-flto

LDFLAGS = \
		-L$(SDL_LIB) \
		-L$(SDL_TTF_LIB) \
		-lSDL2 \
		-lSDL2_ttf \
		-ffunction-sections \
		-fdata-sections \
		-Wl,-gc-sections

#-flto

.PHONY: clean all exe lss

exe: $(OUTDIR)/$(TARGET).exe
lss: $(OUTDIR)/$(TARGET).lss
all: exe lss

# generate .o-files from c-files in src directory
$(OUTDIR)/%.o : %.c
	@echo "compiling $< ..."
	$(CC) $(CFLAGS) -c $< -o $@

# generate .exe file from objects
$(OUTDIR)/$(TARGET).exe: $(OBJS)
	@echo "generating $@ ..."
	$(CC) $(LDFLAGS) $(OBJS) -o $@
	cp $(SDL_LIB)/SDL2.dll $(OUTDIR)/SDL2.dll

# generate .lss file from .exe file
$(OUTDIR)/%.lss: $(OUTDIR)/%.exe
	@echo "generating $@ ..."
	$(OBJDUMP) -d -h -l $< > $@

# -h -l -S

dll:
	$(CC) -m32 -static-libgcc -shared -DBUILD_TEST_DLL=1 -o $(OUTDIR)/$(TARGET).dll $(SRC)

clean:
	rm -rf $(OUTDIR)
