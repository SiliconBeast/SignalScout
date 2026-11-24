// SPDX-License-Identifier: MIT
// Calendar arithmetic for the clock, AOD and logs (UTC, proleptic Gregorian).
#pragma once
#include <stdint.h>
namespace ss { namespace clk {
struct Civil { uint16_t year; uint8_t month, day, hour, minute, second, weekday; }; // weekday 0 = Sunday
uint32_t to_unix(uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s);
Civil from_unix(uint32_t t);
const char* weekday_short(uint8_t wd);
const char* month_short(uint8_t m);
}}
