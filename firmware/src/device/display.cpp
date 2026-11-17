#include "display.h"
#include <Arduino.h>
#include <lvgl.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <string.h>
#include "pins.h"
#include "board.h"

namespace dev {

static spi_device_handle_t spi;
static bool on = false;
// Two 40-row buffers in internal RAM: LVGL renders one while DMA sends the other,
// which is what keeps page slides and the shade drag smooth.
static const size_t LINES = 40;
static const size_t CHUNK = (size_t)SS_LCD_W * LINES * 2; // one flush = one SPI transaction (no reliance on 0x3C continue)
static lv_display_t* disp = nullptr;
static volatile bool dma_busy = false;
static volatile int16_t want_level = -1; // brightness requested by other tasks, applied between flushes
static spi_transaction_t px_trans;
static void IRAM_ATTR on_dma_done(spi_transaction_t* t) { if (t == &px_trans) { dma_busy = false; lv_display_flush_ready(disp); } }

// QSPI framing used by CO5300 / SH8601 class drivers:
//   register write: opcode 0x02, 24-bit address 0x00:CMD:0x00, parameters on one data line
//   pixel write:    opcode 0x32, 24-bit address 0x00:0x2C:0x00 (0x3C to continue), pixels on four lines
static void cmd(uint8_t c, const uint8_t* p = nullptr, size_t n = 0) {
  spi_transaction_t t; memset(&t, 0, sizeof t);
  t.cmd = 0x02; t.addr = (uint32_t)c << 8; t.length = n * 8; t.tx_buffer = n ? p : nullptr;
  spi_device_polling_transmit(spi, &t);
}
static void cmd1(uint8_t c, uint8_t v) { cmd(c, &v, 1); }
static void pixels(const uint8_t* data, size_t bytes) {
  bool first = true;
  while (bytes) {
    size_t n = bytes > CHUNK ? CHUNK : bytes;
    spi_transaction_t t; memset(&t, 0, sizeof t);
    t.flags = SPI_TRANS_MODE_QIO; t.cmd = 0x32; t.addr = first ? 0x002C00 : 0x003C00; t.length = n * 8; t.tx_buffer = data;
    spi_device_polling_transmit(spi, &t);
    data += n; bytes -= n; first = false;
  }
}
static void window(int x1, int y1, int x2, int y2) {
  x1 += SS_LCD_COL_OFFSET; x2 += SS_LCD_COL_OFFSET; y1 += SS_LCD_ROW_OFFSET; y2 += SS_LCD_ROW_OFFSET;
  const uint8_t cx[4] = {(uint8_t)(x1 >> 8), (uint8_t)x1, (uint8_t)(x2 >> 8), (uint8_t)x2};
  const uint8_t cy[4] = {(uint8_t)(y1 >> 8), (uint8_t)y1, (uint8_t)(y2 >> 8), (uint8_t)y2};
  cmd(0x2A, cx, 4); cmd(0x2B, cy, 4);
}
static void flush_cb(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  // The panel wants even-aligned windows; the rounder callback below guarantees that.
  size_t n = (size_t)lv_area_get_width(a) * (size_t)lv_area_get_height(a);
  lv_draw_sw_rgb565_swap(px, (uint32_t)n); // panel is big-endian RGB565
  window(a->x1, a->y1, a->x2, a->y2);
  if (n * 2 > CHUNK) { pixels(px, n * 2); lv_display_flush_ready(d); return; }
  // Queue the pixels and return: the DMA-done callback releases the buffer.
  memset(&px_trans, 0, sizeof px_trans);
  px_trans.flags = SPI_TRANS_MODE_QIO; px_trans.cmd = 0x32; px_trans.addr = 0x002C00; px_trans.length = n * 16; px_trans.tx_buffer = px;
  dma_busy = true;
  if (spi_device_queue_trans(spi, &px_trans, portMAX_DELAY) != ESP_OK) { dma_busy = false; lv_display_flush_ready(d); }
}
static void wait_dma() { spi_transaction_t* r; while (dma_busy) { if (spi_device_get_trans_result(spi, &r, pdMS_TO_TICKS(20)) != ESP_OK) break; } }
static void rounder_cb(lv_event_t* e) { // CO5300 requires even start and odd end coordinates
  lv_area_t* a = (lv_area_t*)lv_event_get_param(e);
  a->x1 &= ~1; a->y1 &= ~1; a->x2 |= 1; a->y2 |= 1;
}

bool display_begin() {
  pinMode(PIN_DISP_RESET, OUTPUT);
  digitalWrite(PIN_DISP_RESET, HIGH); delay(10); digitalWrite(PIN_DISP_RESET, LOW); delay(20); digitalWrite(PIN_DISP_RESET, HIGH); delay(120);

  spi_bus_config_t bus; memset(&bus, 0, sizeof bus);
  bus.data0_io_num = PIN_LCD_D0; bus.data1_io_num = PIN_LCD_D1; bus.data2_io_num = PIN_LCD_D2; bus.data3_io_num = PIN_LCD_D3;
  bus.sclk_io_num = PIN_LCD_SCK; bus.max_transfer_sz = (int)CHUNK + 8; bus.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_QUAD;
  if (spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;
  spi_device_interface_config_t dc; memset(&dc, 0, sizeof dc);
  dc.command_bits = 8; dc.address_bits = 24; dc.mode = 0; dc.clock_speed_hz = SS_LCD_SPI_HZ; dc.spics_io_num = PIN_LCD_CS;
  dc.flags = SPI_DEVICE_HALFDUPLEX; dc.queue_size = 2; dc.post_cb = on_dma_done;
  if (spi_bus_add_device(SPI3_HOST, &dc, &spi) != ESP_OK) return false;

  cmd(0x11); delay(120);          // sleep out
  cmd1(0xFE, 0x00);               // user command page
  cmd1(0xC4, 0x80);               // QSPI interface control
  cmd1(0x3A, 0x55);               // 16 bits per pixel
  cmd1(0x35, 0x00);               // tearing-effect output on (DISP_TE)
  cmd1(0x53, 0x20);               // brightness control on
  cmd1(0x63, 0xFF);               // high-brightness-mode level
  cmd1(0x51, 0x00);               // start dark, HouseSvc sets the real level
  cmd(0x29); delay(20);           // display on
  on = true;

  size_t sz = CHUNK;
  uint8_t* buf = (uint8_t*)heap_caps_malloc(sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  uint8_t* buf2 = (uint8_t*)heap_caps_malloc(sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!buf) return false;
  lv_display_t* d = lv_display_create(SS_LCD_W, SS_LCD_H); disp = d;
  lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(d, buf, buf2, (uint32_t)sz, LV_DISPLAY_RENDER_MODE_PARTIAL); // buf2 may be null: single-buffered fallback
  lv_display_set_flush_cb(d, flush_cb);
  lv_display_add_event_cb(d, rounder_cb, LV_EVENT_INVALIDATE_AREA, nullptr);
  return true;
}
static void apply_brightness(uint8_t pct) {
  if (pct == 0) { if (on) { cmd(0x28); on = false; } return; }
  if (!on) { cmd(0x29); on = true; }
  cmd1(0x51, (uint8_t)(20 + (uint32_t)pct * 235 / 100));
}
void display_brightness(uint8_t pct) { want_level = pct; } // any task; applied by display_poll() on the UI task
void display_poll() {
  if (!spi || want_level < 0 || dma_busy) return;
  int16_t v = want_level; want_level = -1; apply_brightness((uint8_t)v);
}
void display_sleep() { if (!spi) return; wait_dma(); cmd(0x28); cmd(0x10); on = false; delay(5); }
}
