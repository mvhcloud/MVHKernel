#include <stdint.h>
#include "mvh/io.h"
#include "mvh/rtc.h"

static uint8_t cmos_read(uint8_t index)
{
    io_out8(0x70u, index);
    return io_in8(0x71u);
}

static uint8_t from_bcd(uint8_t value)
{
    return (uint8_t)((value & 0x0Fu) + ((value >> 4u) * 10u));
}

void rtc_read(rtc_time_t *time)
{
    uint8_t status_b;
    while ((cmos_read(0x0Au) & 0x80u) != 0u) {
    }
    time->second = cmos_read(0x00u);
    time->minute = cmos_read(0x02u);
    time->hour = cmos_read(0x04u);
    time->day = cmos_read(0x07u);
    time->month = cmos_read(0x08u);
    time->year = cmos_read(0x09u);
    status_b = cmos_read(0x0Bu);
    if ((status_b & 0x04u) == 0u) {
        time->second = from_bcd(time->second);
        time->minute = from_bcd(time->minute);
        time->hour = (uint8_t)((time->hour & 0x80u) | from_bcd(time->hour & 0x7Fu));
        time->day = from_bcd(time->day);
        time->month = from_bcd(time->month);
        time->year = from_bcd((uint8_t)time->year);
    }
    if ((status_b & 0x02u) == 0u && (time->hour & 0x80u) != 0u) {
        time->hour = (uint8_t)(((time->hour & 0x7Fu) + 12u) % 24u);
    }
    time->year = (uint16_t)(2000u + time->year);
}
