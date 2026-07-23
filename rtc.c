#include "rtc.h"
#include "io.h"

static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static int rtc_updating(void)
{
    return cmos_read(0x0A) & 0x80;
}

static uint8_t bcd_to_bin(uint8_t v)
{
    return (uint8_t)((v & 0x0F) + ((v >> 4) * 10));
}

void rtc_read(struct rtc_time *out)
{
    uint8_t second, minute, hour, day, month, year, century = 0;
    uint8_t reg_b;

    while (rtc_updating())
        ;

    second = cmos_read(0x00);
    minute = cmos_read(0x02);
    hour = cmos_read(0x04);
    day = cmos_read(0x07);
    month = cmos_read(0x08);
    year = cmos_read(0x09);
    century = cmos_read(0x32);
    reg_b = cmos_read(0x0B);

    if (!(reg_b & 0x04)) {
        second = bcd_to_bin(second);
        minute = bcd_to_bin(minute);
        hour = bcd_to_bin(hour & 0x7F);
        day = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year = bcd_to_bin(year);
        century = bcd_to_bin(century);
    }

    if (!(reg_b & 0x02) && (hour & 0x80))
        hour = (uint8_t)(((hour & 0x7F) + 12) % 24);

    out->second = second;
    out->minute = minute;
    out->hour = hour;
    out->day = day;
    out->month = month;
    if (century == 0)
        century = 20;
    out->year = (uint16_t)(century * 100 + year);
}
