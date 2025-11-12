include ../../../mk/toolchain.mk

ARCH = -march=rv32izicsr
CROSS_COMPILE = riscv64-unknown-elf-
LINKER_SCRIPT = linker.ld

EMU ?= ../../../build/rv32emu

AFLAGS = -g $(ARCH) -mabi=ilp32
CFLAGS = -g -march=rv32i_zicsr -mabi=ilp32 -ffreestanding
LDFLAGS = -T $(LINKER_SCRIPT)
EXEC = test.elf

CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
OBJDUMP = $(CROSS_COMPILE)objdump

OBJS = start.o main.o perfcounter.o hanoi.o

.PHONY: all run dump clean

all: $(EXEC)

$(EXEC): $(OBJS) $(LINKER_SCRIPT)
	$(CC) $(LDFLAGS) $(CFLAGS) $(OPT_LEVEL) -o $@ $(OBJS) -nostdlib -lgcc

%.o: %.S
	$(AS) $(AFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@ -c

run: $(EXEC)
	@test -f $(EMU) || (echo "Error: $(EMU) not found" && exit 1)
	@grep -q "ENABLE_ELF_LOADER=1" ../../../build/.config || (echo "Error: ENABLE_ELF_LOADER=1 not set" && exit 1)
	@grep -q "ENABLE_SYSTEM=1" ../../../build/.config || (echo "Error: ENABLE_SYSTEM=1 not set" && exit 1)
	$(EMU) $<

dump: $(EXEC)
	$(OBJDUMP) -Ds $< | less

clean:
	rm -f $(EXEC) $(OBJS)

O_LEVELS = Ofast Os

all_multi: $(O_LEVELS:%=test_%.elf)

test_%.elf:
	$(MAKE) clean
	$(MAKE) CFLAGS="-g -$* -march=rv32i_zicsr -mabi=ilp32"
	mv test.elf test_$*.elf
