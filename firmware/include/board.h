// Board-level constants that are not pins (pins: include/pins.h, generated).
#pragma once
// TOQ206XVT-01 2.06" AMOLED. Resolution and driver IC are taken from the
// vendor listing for this panel family (CO5300, 410 x 502, QSPI). VERIFY
// against the panel datasheet during bring-up (docs/BRINGUP.md step 4).
#define SS_LCD_W 410
#define SS_LCD_H 502
#define SS_FW_VERSION "1.0.0"
#define SS_FW_VERSION_BCD 0x0100
