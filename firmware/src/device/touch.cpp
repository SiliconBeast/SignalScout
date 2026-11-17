// Capacitive touch as an LVGL pointer. The panel's touch controller part
// number is not on the BOM yet; FT3168 (0x38) and CST816 (0x15) share this
// register layout, so both are probed. VERIFY axes during bring-up (step 4b).
#include "display.h"
#include <Arduino.h>
#include <lvgl.h>
#include "i2c_dev.h"
#include "board.h"
namespace dev {
static uint8_t addr = 0;
static int16_t lx = 0, ly = 0;
static void read_cb(lv_indev_t*, lv_indev_data_t* d) {
  uint8_t r[5];
  bool down = false;
  // Polled at the LVGL input rate (TP_INT is only a hint) so a release is never missed.
  if (rd(addr, 0x02, r, 5) && (r[0] & 0x0F)) {
    int x = ((r[1] & 0x0F) << 8) | r[2], y = ((r[3] & 0x0F) << 8) | r[4];
    if (x < SS_LCD_W && y < SS_LCD_H) { lx = (int16_t)x; ly = (int16_t)y; down = true; }
  }
  d->point.x = lx; d->point.y = ly; d->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
bool touch_begin() {
  if (present(0x38)) addr = 0x38; else if (present(0x15)) addr = 0x15; else return false;
  lv_indev_t* in = lv_indev_create(); lv_indev_set_type(in, LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(in, read_cb);
  lv_indev_set_scroll_limit(in, 12); lv_indev_set_long_press_time(in, 550);
  return true;
}
}
