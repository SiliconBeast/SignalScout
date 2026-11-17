#pragma once
#include <stdint.h>
namespace ss { namespace battery {
// MAX17048 register conversions.
float vcell_volts(uint16_t reg);        // VCELL (0x02): 78.125 uV / LSB
float soc_percent(uint16_t reg);        // SOC (0x04): 1/256 % / LSB, clamped 0..100
float crate_pct_per_hr(uint16_t reg);   // CRATE (0x16): signed, 0.208 %/hr / LSB
// Fallback when the gauge is absent: open-circuit LiPo curve, 0..100 %.
float percent_from_voltage(float v);
// Minutes to empty (discharging) or to full (charging); -1 if rate is ~0.
int minutes_remaining(float soc, float rate_pct_per_hr);
}}
