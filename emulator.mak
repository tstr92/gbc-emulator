# Makefile for GBC-Emulator

TARGET=emulator

COMPILER = /c/msys64/mingw64/bin
CC = $(COMPILER)/gcc
OBJDUMP = $(COMPILER)/objdump
BIN	= $(COMPILER)/objcopy

OUTDIR = .release

DEBUG?=0

ifeq ($(MAKELEVEL),0)
	dummy := $(shell echo -e "Make Emulator")
	dummy := $(shell mkdir -p $(OUTDIR))
endif

SRC = \
		cpu.c \
		bus.c \
		joypad.c \
		timer.c \
		apu.c

OBJS = $(addprefix $(OUTDIR)/,$(SRC:.c=.o))

CFLAGS = \
		-DDEBUG=$(DEBUG) \
		-DUSE_0xE000_AS_PUTC_DEVICE=1 \
		-ffunction-sections \
		-fdata-sections \
		-gdwarf-2 \
		-O2

LDFLAGS = \
		-ffunction-sections \
		-fdata-sections \
		-Wl,-gc-sections

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
	$(CC) $(LDFLAGS) $(OBJS) -o $@

# generate .lss file from .exe file
$(OUTDIR)/%.lss: $(OUTDIR)/%.exe
	@echo "generating $@ ..."
	$(OBJDUMP) -D -h -l -S $< > $@

# -h -l -S

dll:
	$(CC) -m32 -static-libgcc -shared -DBUILD_TEST_DLL=1 -o $(OUTDIR)/$(TARGET).dll $(SRC)

clean:
	rm -rf $(OUTDIR)
