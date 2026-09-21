/*
 * rtc.c -- the CMOS real-time clock.
 *
 * Reading the RTC has two traps. The chip may be mid-update when we look,
 * which yields a mixture of the old and new time; and depending on how
 * the firmware configured it, the values may be BCD rather than binary,
 * and the hour may carry a 12-hour PM flag. We handle all three.
 */

#include "rtc.h"
#include "io.h"

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71

#define RTC_SECOND  0x00
#define RTC_MINUTE  0x02
#define RTC_HOUR    0x04
#define RTC_DAY     0x07
#define RTC_MONTH   0x08
#define RTC_YEAR    0x09
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B

#define STATUS_A_UPDATING 0x80
#define STATUS_B_BINARY   0x04
#define STATUS_B_24HOUR   0x02

static uint8_t cmos_read(uint8_t reg)
{
    /* Bit 7 of the index port controls NMI masking; leave it clear. */
    outb(CMOS_INDEX, reg & 0x7F);
    io_wait();
    return inb(CMOS_DATA);
}

static bool rtc_updating(void)
{
    return (cmos_read(RTC_STATUS_A) & STATUS_A_UPDATING) != 0;
}

static uint8_t bcd_to_binary(uint8_t value)
{
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

static void rtc_read_raw(struct rtc_time *out)
{
    out->second = cmos_read(RTC_SECOND);
    out->minute = cmos_read(RTC_MINUTE);
    out->hour   = cmos_read(RTC_HOUR);
    out->day    = cmos_read(RTC_DAY);
    out->month  = cmos_read(RTC_MONTH);
    out->year   = cmos_read(RTC_YEAR);
}

void rtc_read(struct rtc_time *out)
{
    struct rtc_time previous;
    uint32_t guard = 0;

    /*
     * Read twice and accept the result only when two consecutive reads
     * agree, which means no update landed between them. The guard keeps
     * a stuck or absent RTC from hanging the caller.
     */
    while (rtc_updating() && guard++ < 1000000)
        ;

    rtc_read_raw(out);

    do {
        previous = *out;

        while (rtc_updating() && guard++ < 1000000)
            ;

        rtc_read_raw(out);
    } while ((previous.second != out->second ||
              previous.minute != out->minute ||
              previous.hour   != out->hour   ||
              previous.day    != out->day    ||
              previous.month  != out->month  ||
              previous.year   != out->year) && guard++ < 1000000);

    uint8_t status_b = cmos_read(RTC_STATUS_B);
    bool    is_pm    = (out->hour & 0x80) != 0;

    if (!(status_b & STATUS_B_BINARY)) {
        out->second = bcd_to_binary(out->second);
        out->minute = bcd_to_binary(out->minute);
        /* Mask off the PM flag before converting the hour. */
        out->hour   = bcd_to_binary((uint8_t)(out->hour & 0x7F));
        out->day    = bcd_to_binary(out->day);
        out->month  = bcd_to_binary(out->month);
        out->year   = bcd_to_binary((uint8_t)out->year);
    } else {
        out->hour &= 0x7F;
    }

    /* Convert a 12-hour clock to 24-hour: 12 AM is 0, 12 PM stays 12. */
    if (!(status_b & STATUS_B_24HOUR)) {
        if (out->hour == 12)
            out->hour = 0;
        if (is_pm)
            out->hour = (uint8_t)(out->hour + 12);
    }

    /* The CMOS only stores two digits; the century register is not
     * reliable across firmware, so assume 2000-2099. */
    out->year += 2000;
}
