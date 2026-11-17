#include "clock.h"
namespace ss { namespace clk {
// Howard Hinnant's days_from_civil / civil_from_days.
static int32_t days_from_civil(int32_t y, uint32_t m, uint32_t d) {
  y -= m <= 2; int32_t era = (y >= 0 ? y : y - 399) / 400; uint32_t yoe = (uint32_t)(y - era * 400);
  uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}
uint32_t to_unix(uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s) {
  return (uint32_t)days_from_civil(y, mo, d) * 86400u + h * 3600u + mi * 60u + s;
}
Civil from_unix(uint32_t t) {
  Civil c; int32_t z = (int32_t)(t / 86400u); uint32_t sod = t % 86400u;
  c.weekday = (uint8_t)((z + 4) % 7);
  z += 719468; int32_t era = z / 146097; uint32_t doe = (uint32_t)(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; int32_t y = (int32_t)yoe + era * 400;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100); uint32_t mp = (5 * doy + 2) / 153;
  c.day = (uint8_t)(doy - (153 * mp + 2) / 5 + 1); c.month = (uint8_t)(mp < 10 ? mp + 3 : mp - 9); c.year = (uint16_t)(y + (c.month <= 2));
  c.hour = (uint8_t)(sod / 3600); c.minute = (uint8_t)(sod / 60 % 60); c.second = (uint8_t)(sod % 60);
  return c;
}
const char* weekday_short(uint8_t wd) { static const char* n[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}; return n[wd % 7]; }
const char* month_short(uint8_t m) { static const char* n[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}; return n[(m + 11) % 12]; }
}}
