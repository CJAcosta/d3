# Nebula OS build system.
#
#   make          build the bootable floppy image (build/nebula.img)
#   make run      boot the image in QEMU with a graphical window
#   make run-text boot headless, kernel console on stdout
#   make test     run the automated boot test suite
#   make clean    remove build artifacts

CC      := gcc
LD      := ld
OBJCOPY := objcopy
QEMU    := qemu-system-i386

BUILD   := build
SRCDIR  := kernel
INCDIR  := include

# -nostdinc and -ffreestanding keep the host's libc out of a kernel that
# has no libc. No SSE/MMX: we never save those registers on a context
# switch, so the compiler must not use them behind our back.
CFLAGS := -m32 -std=gnu11 -O2 \
          -ffreestanding -fno-builtin -nostdlib -nostdinc \
          -fno-stack-protector -fno-pic -fno-pie \
          -fno-asynchronous-unwind-tables -fno-omit-frame-pointer \
          -mno-sse -mno-sse2 -mno-mmx -mno-red-zone \
          -Wall -Wextra -Wno-unused-parameter \
          -I$(INCDIR)

ASFLAGS := -m32 -I$(INCDIR)
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib -z noexecstack

C_SOURCES   := $(wildcard $(SRCDIR)/*.c)
ASM_SOURCES := $(wildcard $(SRCDIR)/*.S)

# entry.o must link first so _start lands at the load address.
OBJECTS := $(BUILD)/entry.o \
           $(patsubst $(SRCDIR)/%.S,$(BUILD)/%.o,$(filter-out $(SRCDIR)/entry.S,$(ASM_SOURCES))) \
           $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(C_SOURCES))

IMAGE  := $(BUILD)/nebula.img
KERNEL := $(BUILD)/kernel.bin
BOOT   := $(BUILD)/boot.bin

.PHONY: all clean run run-text test unit-test check debug
.DELETE_ON_ERROR:

all: $(IMAGE)

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: $(SRCDIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SRCDIR)/%.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

$(KERNEL): $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# The boot sector needs to know how many sectors to pull off the disk, so
# it can only be assembled once the kernel's final size is known.
$(BOOT): boot/boot.S $(KERNEL) | $(BUILD)
	@sectors=$$(( ( $$(stat -c%s $(KERNEL)) + 511 ) / 512 )); \
	if [ $$sectors -gt 1024 ]; then \
	    echo "error: kernel is $$sectors sectors; too large for this loader" >&2; \
	    exit 1; \
	fi; \
	echo "$(CC) $(ASFLAGS) -DKERNEL_SECTORS=$$sectors -c boot/boot.S"; \
	$(CC) $(ASFLAGS) -DKERNEL_SECTORS=$$sectors -c boot/boot.S -o $(BUILD)/boot.o
	$(LD) -m elf_i386 -Ttext 0x7C00 -e _start --oformat binary \
	      -o $@ $(BUILD)/boot.o
	@size=$$(stat -c%s $@); \
	if [ $$size -ne 512 ]; then \
	    echo "error: boot sector is $$size bytes, must be exactly 512" >&2; \
	    exit 1; \
	fi

# A 1.44 MiB floppy image: boot sector first, kernel immediately after.
$(IMAGE): $(BOOT) $(KERNEL)
	@dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	@dd if=$(BOOT) of=$@ conv=notrunc status=none
	@dd if=$(KERNEL) of=$@ bs=512 seek=1 conv=notrunc status=none
	@echo "built $@ (kernel: $$(stat -c%s $(KERNEL)) bytes)"

run: $(IMAGE)
	$(QEMU) -fda $(IMAGE) -serial stdio -no-reboot

run-text: $(IMAGE)
	$(QEMU) -fda $(IMAGE) -display none -serial stdio -no-reboot

debug: $(IMAGE)
	$(QEMU) -fda $(IMAGE) -display none -serial stdio -no-reboot -s -S

test: $(IMAGE)
	@scripts/run-tests.sh

clean:
	rm -rf $(BUILD)

# Host-side unit tests for the kernel's hardware-independent logic.
# Compiled for the host (still -m32, so the kernel's 32-bit type widths
# hold) and linked against libc, with kmalloc and friends redirected to
# malloc by the test harness.
HOST_TEST_SOURCES := tests/host_tests.c $(SRCDIR)/string.c \
                     $(SRCDIR)/kprintf.c $(SRCDIR)/fs.c

$(BUILD)/host_tests: $(HOST_TEST_SOURCES) | $(BUILD)
	$(CC) -m32 -std=gnu11 -g -O1 -Wall -Wextra -Wno-unused-parameter \
	      -fno-builtin -I$(INCDIR) -o $@ $(HOST_TEST_SOURCES)

unit-test: $(BUILD)/host_tests
	@$(BUILD)/host_tests

# Everything: fast host tests first, then the full boot tests.
check: unit-test test
