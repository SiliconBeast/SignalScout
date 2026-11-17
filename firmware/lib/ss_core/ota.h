// Over-the-air updates: release manifest and version ordering.
// A release publishes firmware.bin plus manifest.json (tools/make_release.py):
//   {"version":"1.2.0","channel":"stable","url":"https://.../firmware.bin",
//    "size":1234567,"sha256":"<64 hex>","min_version":"1.0.0","notes":"..."}
// The device downloads to the inactive app slot, checks size + SHA-256, boots
// it, and marks it valid only after its own self-test (see BRINGUP.md).
#pragma once
#include <stdint.h>
namespace ss { namespace ota {

static const char* const MANIFEST_URL = "https://github.com/SiliconBeast/SignalScout/releases/latest/download/manifest.json";

struct Manifest { char version[16]; char channel[10]; char url[192]; char sha256[65]; uint32_t size; char min_version[16]; char notes[96]; };
bool parse_manifest(const char* json, Manifest& m);
int semver_cmp(const char* a, const char* b); // <0, 0, >0; "1.10.0" > "1.9.3"; missing parts = 0
// Should this device offer the update? (newer, and current >= min_version)
bool should_offer(const Manifest& m, const char* current);
}}
