#include <stdint.h>
#include "mvh/ata.h"
#include "mvh/block.h"
#include "mvh/io.h"
#include "mvh/sync.h"

#define ATA_MAX_DEVICES 4u
#define ATA_STATUS_ERR 0x01u
#define ATA_STATUS_DRQ 0x08u
#define ATA_STATUS_DF 0x20u
#define ATA_STATUS_BSY 0x80u
#define ATA_CMD_READ_SECTORS 0x20u
#define ATA_CMD_WRITE_SECTORS 0x30u
#define ATA_CMD_CACHE_FLUSH 0xE7u
#define ATA_CMD_IDENTIFY 0xECu
#define ATA_POLL_LIMIT 1000000u

typedef struct {
    ata_device_info_t info;
    spinlock_t lock;
} ata_device_t;

static ata_device_t devices[ATA_MAX_DEVICES];
static uint32_t device_count;

static void delay_400ns(const ata_device_t *device)
{
    (void)io_in8(device->info.control_base);
    (void)io_in8(device->info.control_base);
    (void)io_in8(device->info.control_base);
    (void)io_in8(device->info.control_base);
}

static int wait_not_busy(const ata_device_t *device)
{
    uint32_t timeout;
    for (timeout = 0u; timeout < ATA_POLL_LIMIT; timeout++) {
        uint8_t status = io_in8((uint16_t)(device->info.io_base + 7u));
        if ((status & ATA_STATUS_BSY) == 0u) return status;
    }
    return -1;
}

static int wait_data(const ata_device_t *device)
{
    uint32_t timeout;
    for (timeout = 0u; timeout < ATA_POLL_LIMIT; timeout++) {
        uint8_t status = io_in8((uint16_t)(device->info.io_base + 7u));
        if ((status & (ATA_STATUS_ERR | ATA_STATUS_DF)) != 0u) return -1;
        if ((status & ATA_STATUS_BSY) == 0u && (status & ATA_STATUS_DRQ) != 0u) return 0;
    }
    return -1;
}

static int select_lba28(const ata_device_t *device, uint64_t lba)
{
    uint16_t base = device->info.io_base;
    if (lba > 0x0FFFFFFFull) return -1;
    io_out8((uint16_t)(base + 6u),
            (uint8_t)(0xE0u | (device->info.drive << 4u) | ((lba >> 24u) & 0x0Fu)));
    delay_400ns(device);
    if (wait_not_busy(device) < 0) return -1;
    io_out8((uint16_t)(base + 2u), 1u);
    io_out8((uint16_t)(base + 3u), (uint8_t)lba);
    io_out8((uint16_t)(base + 4u), (uint8_t)(lba >> 8u));
    io_out8((uint16_t)(base + 5u), (uint8_t)(lba >> 16u));
    return 0;
}

static int ata_read(void *context, uint64_t lba, uint32_t count, void *buffer)
{
    ata_device_t *device = (ata_device_t *)context;
    uint16_t *target = (uint16_t *)buffer;
    uint32_t sector;
    uint32_t word;
    spinlock_lock(&device->lock);
    for (sector = 0u; sector < count; sector++) {
        if (select_lba28(device, lba + sector) != 0) goto fail;
        io_out8((uint16_t)(device->info.io_base + 7u), ATA_CMD_READ_SECTORS);
        if (wait_data(device) != 0) goto fail;
        for (word = 0u; word < 256u; word++)
            target[sector * 256u + word] = io_in16(device->info.io_base);
        delay_400ns(device);
    }
    spinlock_unlock(&device->lock);
    return 0;
fail:
    spinlock_unlock(&device->lock);
    return -1;
}

static int ata_write(void *context, uint64_t lba, uint32_t count, const void *buffer)
{
    ata_device_t *device = (ata_device_t *)context;
    const uint16_t *source = (const uint16_t *)buffer;
    uint32_t sector;
    uint32_t word;
    spinlock_lock(&device->lock);
    for (sector = 0u; sector < count; sector++) {
        if (select_lba28(device, lba + sector) != 0) goto fail;
        io_out8((uint16_t)(device->info.io_base + 7u), ATA_CMD_WRITE_SECTORS);
        if (wait_data(device) != 0) goto fail;
        for (word = 0u; word < 256u; word++)
            io_out16(device->info.io_base, source[sector * 256u + word]);
    }
    io_out8((uint16_t)(device->info.io_base + 7u), ATA_CMD_CACHE_FLUSH);
    if (wait_not_busy(device) < 0) goto fail;
    spinlock_unlock(&device->lock);
    return 0;
fail:
    spinlock_unlock(&device->lock);
    return -1;
}

static int identify(uint8_t channel, uint8_t drive, ata_device_t *device)
{
    static const uint16_t io_bases[2] = {0x1F0u, 0x170u};
    static const uint16_t control_bases[2] = {0x3F6u, 0x376u};
    uint16_t words[256];
    uint16_t base = io_bases[channel];
    uint8_t status;
    uint32_t index;
    uint64_t sectors;
    io_out8((uint16_t)(base + 6u), (uint8_t)(0xA0u | (drive << 4u)));
    for (index = 0u; index < 4u; index++) (void)io_in8(control_bases[channel]);
    io_out8((uint16_t)(base + 2u), 0u);
    io_out8((uint16_t)(base + 3u), 0u);
    io_out8((uint16_t)(base + 4u), 0u);
    io_out8((uint16_t)(base + 5u), 0u);
    io_out8((uint16_t)(base + 7u), ATA_CMD_IDENTIFY);
    status = io_in8((uint16_t)(base + 7u));
    if (status == 0u || status == 0xFFu) return -1;
    device->info.io_base = base;
    device->info.control_base = control_bases[channel];
    if (wait_not_busy(device) < 0 || io_in8((uint16_t)(base + 4u)) != 0u ||
        io_in8((uint16_t)(base + 5u)) != 0u || wait_data(device) != 0) return -1;
    for (index = 0u; index < 256u; index++) words[index] = io_in16(base);
    sectors = (uint64_t)words[60] | ((uint64_t)words[61] << 16u);
    if (sectors == 0u) return -1;
    if (sectors > 0x10000000ull) sectors = 0x10000000ull;
    device->info.present = 1u;
    device->info.channel = channel;
    device->info.drive = drive;
    device->info.lba48 = (words[83] & (1u << 10u)) != 0u;
    device->info.writable = 1u;
    device->info.sectors = sectors;
    for (index = 0u; index < 20u; index++) {
        device->info.model[index * 2u] = (char)(words[27u + index] >> 8u);
        device->info.model[index * 2u + 1u] = (char)words[27u + index];
    }
    device->info.model[40] = '\0';
    spinlock_init(&device->lock);
    return 0;
}

uint32_t ata_init(void)
{
    uint8_t channel;
    uint8_t drive;
    device_count = 0u;
    for (channel = 0u; channel < 2u; channel++) {
        for (drive = 0u; drive < 2u; drive++) {
            ata_device_t candidate;
            uint8_t *bytes = (uint8_t *)&candidate;
            uint32_t index;
            char name[5] = {'a', 't', 'a', '0', '\0'};
            for (index = 0u; index < sizeof(candidate); index++) bytes[index] = 0u;
            if (identify(channel, drive, &candidate) != 0) continue;
            if (device_count >= ATA_MAX_DEVICES) return device_count;
            devices[device_count] = candidate;
            spinlock_init(&devices[device_count].lock);
            name[3] = (char)('0' + device_count);
            if (block_register(name, 512u, devices[device_count].info.sectors, 1u,
                               &devices[device_count], ata_read, ata_write) >= 0)
                device_count++;
        }
    }
    return device_count;
}

uint32_t ata_device_count(void)
{
    return device_count;
}

int ata_device_info(uint32_t index, ata_device_info_t *info)
{
    if (info == 0 || index >= device_count) return -1;
    *info = devices[index].info;
    return 0;
}
