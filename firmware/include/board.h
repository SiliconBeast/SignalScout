// Board-level constants that are not pins (pins: include/pins.h, generated).
#pragma once
// TOQ206XVT-01 2.06" AMOLED. Resolution and driver IC are taken from the
// vendor listing for this panel family (CO5300, 410 x 502, QSPI). VERIFY
// against the panel datasheet during bring-up (docs/BRINGUP.md step 4).
#define SS_LCD_W 410
#define SS_LCD_H 502
#define SS_FW_VERSION "1.0.0"
#define SS_FW_VERSION_BCD 0x0100
// CO5300 RAM is 480 columns wide; the 410-column glass starts at column 22.
#define SS_LCD_COL_OFFSET 22
#define SS_LCD_ROW_OFFSET 0
#define SS_LCD_SPI_HZ 40000000
// u-blox M10 factory default UART baud rate.
#define SS_GNSS_BAUD 9600
#define SS_SLED_BAUD 115200
