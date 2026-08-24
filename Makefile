CC := gcc
LD := ld

BUILD := build
CFLAGS := -m64 -mno-red-zone -std=c11 -ffreestanding -fno-pie -fstack-protector-strong -mstack-protector-guard=global -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-omit-frame-pointer -Wall -Wextra -Werror -O2 -MMD -MP -Iinclude
C_SOURCES := $(wildcard src/*.c src/*/*.c)
ASM_SOURCES := $(wildcard src/*.S src/*/*.S)
C_OBJECTS := $(addprefix $(BUILD)/,$(notdir $(C_SOURCES:.c=.o)))
ASM_OBJECTS := $(addprefix $(BUILD)/,$(notdir $(ASM_SOURCES:.S=.o)))
OBJECTS := $(ASM_OBJECTS) $(C_OBJECTS)
DEPS := $(C_OBJECTS:.o=.d)

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.S $(sort $(dir $(ASM_SOURCES)))
HOST_TEST := $(BUILD)/host-storage-test
HOST_UTIL_TEST := $(BUILD)/host-util-test
HOST_MVHFS_TEST := $(BUILD)/host-mvhfs-test
HOST_NET_TEST := $(BUILD)/host-net-test

.DELETE_ON_ERROR:

.PHONY: all clean host-test iso system-iso

all: $(BUILD)/kernel.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	$(CC) -m64 -c $< -o $@

$(BUILD)/kernel.elf: $(OBJECTS) linker.ld
	$(LD) -m elf_x86_64 -T linker.ld -nostdlib -o $@ $(OBJECTS)

$(HOST_TEST): tests/host_storage_test.c src/firmware/acpi.c src/firmware/smbios.c src/core/bootinfo.c src/core/crc32.c src/core/sync.c src/storage/block.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -Iinclude $^ -o $@

$(HOST_UTIL_TEST): tests/host_util_test.c src/core/util.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -Iinclude $^ -o $@

$(HOST_MVHFS_TEST): tests/host_mvhfs_test.c src/fs/mvhfs.c src/core/crc32.c src/core/sync.c src/storage/block.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -Iinclude $^ -o $@

$(HOST_NET_TEST): tests/host_net_test.c src/net/net.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -Iinclude $^ -o $@

host-test: $(HOST_TEST) $(HOST_UTIL_TEST) $(HOST_MVHFS_TEST) $(HOST_NET_TEST)
	./$(HOST_TEST)
	./$(HOST_UTIL_TEST)
	./$(HOST_MVHFS_TEST)
	./$(HOST_NET_TEST)

iso: $(BUILD)/kernel.elf config/grub.cfg
	mkdir -p $(BUILD)/iso/boot/grub
	cp $(BUILD)/kernel.elf $(BUILD)/iso/boot/kernel.elf
	cp config/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/mvh-kernel.iso $(BUILD)/iso

system-iso: $(BUILD)/kernel.elf config/grub-system.cfg
	mkdir -p $(BUILD)/system-iso/boot/grub
	cp $(BUILD)/kernel.elf $(BUILD)/system-iso/boot/kernel.elf
	cp config/grub-system.cfg $(BUILD)/system-iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/mvh-betriebsystem.iso $(BUILD)/system-iso

clean:
	rm -rf $(BUILD)

-include $(DEPS)
