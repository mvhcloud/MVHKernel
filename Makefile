CC := gcc
LD := ld

BUILD := build
CFLAGS := -m64 -mno-red-zone -std=c11 -ffreestanding -fno-pie -fstack-protector-strong -mstack-protector-guard=global -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-omit-frame-pointer -Wall -Wextra -Werror -O2 -MMD -MP -Iinclude
OBJECTS := $(BUILD)/boot32.o $(BUILD)/entry64.o $(BUILD)/interrupt64.o $(BUILD)/interrupt.o $(BUILD)/hal.o $(BUILD)/bootinfo.o $(BUILD)/acpi.o $(BUILD)/smbios.o $(BUILD)/log.o $(BUILD)/panic.o $(BUILD)/stack_guard.o $(BUILD)/util.o $(BUILD)/crc32.o $(BUILD)/random.o $(BUILD)/sync.o $(BUILD)/device.o $(BUILD)/pmm.o $(BUILD)/vmm.o $(BUILD)/heap.o $(BUILD)/task.o $(BUILD)/kernel.o $(BUILD)/vga.o $(BUILD)/serial.o $(BUILD)/keyboard.o $(BUILD)/cpu.o $(BUILD)/rtc.o $(BUILD)/pci.o $(BUILD)/timer.o $(BUILD)/ramfs.o $(BUILD)/vfs.o $(BUILD)/mvhfs.o $(BUILD)/block.o
DEPS := $(OBJECTS:.o=.d)
HOST_TEST := $(BUILD)/host-storage-test
HOST_UTIL_TEST := $(BUILD)/host-util-test
HOST_MVHFS_TEST := $(BUILD)/host-mvhfs-test

.DELETE_ON_ERROR:

.PHONY: all clean host-test iso

all: $(BUILD)/kernel.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot32.o: src/boot32.S | $(BUILD)
	$(CC) -m64 -c $< -o $@

$(BUILD)/entry64.o: src/entry64.S | $(BUILD)
	$(CC) -m64 -c $< -o $@

$(BUILD)/interrupt64.o: src/interrupt64.S | $(BUILD)
	$(CC) -m64 -c $< -o $@

$(BUILD)/interrupt.o: src/arch/interrupt.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/hal.o: src/hal/hal.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/bootinfo.o: src/core/bootinfo.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/acpi.o: src/firmware/acpi.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/smbios.o: src/firmware/smbios.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/log.o: src/core/log.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/panic.o: src/core/panic.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/stack_guard.o: src/core/stack_guard.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/util.o: src/core/util.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/crc32.o: src/core/crc32.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/random.o: src/core/random.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sync.o: src/core/sync.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/device.o: src/device/device.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pmm.o: src/memory/pmm.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vmm.o: src/memory/vmm.c | $(BUILD)
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

$(BUILD)/mvhfs.o: src/fs/mvhfs.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/block.o: src/storage/block.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJECTS) linker.ld
	$(LD) -m elf_x86_64 -T linker.ld -nostdlib -o $@ $(OBJECTS)

$(HOST_TEST): tests/host_storage_test.c src/firmware/acpi.c src/firmware/smbios.c src/core/bootinfo.c src/core/crc32.c src/core/sync.c src/storage/block.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -Iinclude $^ -o $@

$(HOST_UTIL_TEST): tests/host_util_test.c src/core/util.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -Iinclude $^ -o $@

$(HOST_MVHFS_TEST): tests/host_mvhfs_test.c src/fs/mvhfs.c src/core/crc32.c src/core/sync.c src/storage/block.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -Iinclude $^ -o $@

host-test: $(HOST_TEST) $(HOST_UTIL_TEST) $(HOST_MVHFS_TEST)
	./$(HOST_TEST)
	./$(HOST_UTIL_TEST)
	./$(HOST_MVHFS_TEST)

iso: $(BUILD)/kernel.elf config/grub.cfg
	mkdir -p $(BUILD)/iso/boot/grub
	cp $(BUILD)/kernel.elf $(BUILD)/iso/boot/kernel.elf
	cp config/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/mvh-kernel.iso $(BUILD)/iso

clean:
	rm -rf $(BUILD)

-include $(DEPS)
