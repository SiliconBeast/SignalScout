// Device implementations of the HAL interfaces (ESP32-S3 base board).
#pragma once
#include "hal.h"
namespace dev {
// Brings up buses and every peripheral, fills `hal`. Returns a bit mask of
// parts that did not answer (see BootFault) so main can report them.
enum BootFault : uint32_t { F_EXPANDER = 1, F_GAUGE = 2, F_HAPTIC = 4, F_BME = 8, F_IMU = 16, F_DISPLAY = 32, F_SD = 64, F_TOUCH = 128 };
uint32_t hal_begin(ss::Hal& hal);
}
