// Network side of the base board: INet implementation, hotspot web console,
// OTA boot handling. See docs/SIGNALOS.md.
#pragma once
#include "hal.h"
#include "bus.h"
namespace ss { struct Shared; }
namespace dev {
ss::INet* net_device();
void net_attach(ss::Bus* bus, ss::Shared* shared, ss::IStorage* storage); // for the web console
void net_poll();                       // web console requests (call every few ms)
void sd_lock(); void sd_unlock();      // the SD card is shared by StoreSvc and the web console
void ota_boot_check(bool safe_mode_key); // B held at power-on: boot the previous firmware
void ota_mark_good();                  // after the self-test: cancel the automatic rollback
}
