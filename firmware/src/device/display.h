// CO5300 AMOLED over QSPI (ESP-IDF spi_master) and its LVGL display driver.
#pragma once
#include <stdint.h>
namespace dev {
bool display_begin();            // panel init + LVGL display registration (call after lv_init)
void display_brightness(uint8_t pct); // 0 = display off; safe from any task
void display_poll();             // UI task: applies brightness changes between DMA flushes
bool touch_begin();              // capacitive touch (FT3168 / CST816 class, I2C) as an LVGL pointer
void display_sleep();
}
