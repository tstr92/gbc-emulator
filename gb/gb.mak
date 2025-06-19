# Makefile gb

TARGET=gb

COMPILER = "/c/Program Files (x86)/SDCC/bin"
MAKEBIN = $(COMPILER)/makebin
CC = $(COMPILER)/sdcc

OUTDIR = .$(TARGET)

ifeq ($(MAKELEVEL),0)
	useless := $(shell echo -e "Make gb")
	useless := $(shell mkdir -p $(OUTDIR))
endif

SRC = gb.c\

CFLAGS = -msm83 \

LDFLAGS = \

.PHONY: all compile bin clean

all: compile bin

compile:
	@echo "compiling $< ..."
	$(CC) $(CFLAGS) $(SRC) -o $(OUTDIR)/$(TARGET).ihx

bin: compile $(OUTDIR)/gb.bin

$(OUTDIR)/gb.bin: $(OUTDIR)/gb.ihx
	@echo "generating $@ ..."
	$(MAKEBIN) $< $@

clean:
	rm -rf $(OUTDIR)
