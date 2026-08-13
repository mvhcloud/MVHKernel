CC := gcc
LD := ld

BUILD := build
CFLAGS := -m64 -mno-red-zone -std=c11 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -O2 -Iinclude
OBJECTS := $(BUILD)/entry64.o $(BUILD)/interrupt64.o $(BUILD)/interrupt.o $(BUILD)/hal.o $(BUILD)/pmm.o $(BUILD)/heap.o $(BUILD)/task.o $(BUILD)/kernel.o $(BUILD)/vga.o $(BUILD)/serial.o $(BUILD)/keyboard.o $(BUILD)/cpu.o $(BUILD)/rtc.o $(BUILD)/pci.o $(BUILD)/timer.o $(BUILD)/ramfs.o $(BUILD)/vfs.o

.PHONY: all clean

all: $(BUILD)/kernel.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/entry64.o: src/entry64.S | $(BUILD)
	$(CC) -m64 -c $< -o $@

$(BUILD)/interrupt64.o: src/interrupt64.S | $(BUILD)
	$(CC) -m64 -c $< -o $@

$(BUILD)/interrupt.o: src/arch/interrupt.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/hal.o: src/hal/hal.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pmm.o: src/memory/pmm.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/heap.o: src/memory/heap.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/task.o: src/task/task.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.o: src/kernel.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vga.o: src/drivers/vga.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/serial.o: src/drivers/serial.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/keyboard.o: src/drivers/keyboard.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cpu.o: src/drivers/cpu.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/rtc.o: src/drivers/rtc.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pci.o: src/drivers/pci.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/timer.o: src/drivers/timer.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ramfs.o: src/fs/ramfs.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vfs.o: src/fs/vfs.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJECTS) linker.ld
	$(LD) -m elf_x86_64 -T linker.ld -nostdlib -o $@ $(OBJECTS)

clean:
	rm -rf $(BUILD)
