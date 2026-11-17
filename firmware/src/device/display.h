// CO5300 AMOLED over QSPI (ESP-IDF spi_master) and its LVGL display driver.
#pragma once
#include <stdint.h>
namespace dev {
bool display_begin();            // panel init + LVGL display registration (call after lv_init)
void display_brightness(uint8_t pct); // 0 = display off
void display_sleep();
}
