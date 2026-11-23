# Base board review and rev C7 rework

Checked against the netlist in `SignalScout_Base/` (rev C.5 placement, same nets as the routed C6b) and the AW9523B and BQ24074 datasheets.

## Verdicts

| # | Item | Verdict | Action |
|---|---|---|---|
| 1 | **RGB LED has no series resistors** | **Fix on the board.** The AW9523B powers up with every pin as an output driven low (address 0x58 straps AD1/AD0 low; registers 04H/05H default to output, 13H to GPIO mode). So the LED cathodes are pulled low with nothing limiting the current from power-on until the firmware runs. That includes the whole time the ESP32 sits in the bootloader or is being flashed. | Rework R50 to R52. The firmware now switches the LED pins to current-sink mode as its very first expander write. |
| 2 | `SD_CS` on IO0 with the BOOT button | **OK.** R9 pulls IO0 high, so it boots normally. SW10 sits inside the enclosure (no cap), and flashing over USB-C doesn't need it: the USB Serial/JTAG controller resets into the bootloader by itself. A press can only happen with the case open. | None. Don't press BOOT while the unit is running. |
| 3 | Sled UART on UART0, boot ROM log on IO43 | **OK.** Every frame has a sync word and a CRC, so the sled parser drops the ROM text. The ESP32 ROM only listens on IO44 in download mode, which needs IO0 low. | None. Optional and permanent: `espefuse.py burn_efuse UART_PRINT_CONTROL 3` silences the ROM log. |
| 4 | **No VBUS sense** | **Fix on the board.** BQ24074 `PGOOD` (pin 7, open-drain, low when input power is valid) is unconnected. | Rework: PGOOD to `SPARE_IO`. |
| 5 | GNSS supply not switched | **OK as designed.** `VCC`, `V_IO` and `V_BCKP` all sit on the always-on +3V3. Before deep sleep the firmware sends UBX-RXM-PMREQ (backup), and the receiver keeps its almanac for a fast restart. The only open point is the measured sleep current. | Measure in bring-up step 11. If it's too high, a load switch goes in rev D. |
| 6 | **IMU interrupts unconnected** | **Fix on the board.** Without an IMU interrupt there is no wake on a fall, which the planned fall-detection and auto-SOS feature needs. | Rework: LSM6DSO INT2 to `SPARE_IO` through 1 kΩ. |

Also checked:
- **BQ24074 pin 15 is ITERM, not SYSOFF.** Left open it gives the default 10 % termination current. Pin 14 (TMR) open gives the default safety timers. CE is low, and EN2=1, EN1=0 means the ILIM resistor sets the input current. All correct.
- **`CHG_TS`:** R3 (10 k fixed) is do-not-populate and J9 is the cell's 10 k NTC, as the schematic note says ("never both"). With both fitted the charger would see about 5 kΩ, read that as a hot battery and stop charging. Keep R3 unfitted.
- **`EXP_RST`:** R29 pulls it up, which beats the AW9523B's internal 100 kΩ pull-down. OK.
- **Key lines:** each has its own 10 k pull-up (R31 to R39); the AW9523B has no internal pull-ups. OK.

## Rev C7 rework (C6b to C7)

Three changes, all near U4, U6, U10 and LED1. Do the schematic first, then Update PCB from Schematic and route the short new nets.

| Ref | Part | From | To | Why |
|---|---|---|---|---|
| R50 | 82 Ω 0402 | LED1 pin 2 (red cathode) | U4 pin 15 (`LED_RK`, P1_5) | Limits the power-on current to about 14 mA |
| R51 | 22 Ω 0402 | LED1 pin 3 (green cathode) | U4 pin 16 (`LED_GK`, P1_6) | Same; small enough to keep current-sink headroom at 3.3 V |
| R52 | 22 Ω 0402 | LED1 pin 4 (blue cathode) | U4 pin 17 (`LED_BK`, P1_7) | Same |
| R53 | 1 kΩ 0402 | U6 pin 9 (`INT2`, net `IMU_INT`) | `SPARE_IO` | Fall interrupt; limits current before the firmware sets the INT pins open-drain. INT1 (pin 4) is boxed in by `LORA_BUSY` and a GND trace, so INT2 is used. |
| (trace) | | U10 pin 7 (`PGOOD`) | `SPARE_IO` | USB/adapter present |

`SPARE_IO` keeps R30 (10 k to +3V3) as its pull-up and still goes to J7 pin 5. Label J7 pin 5 as `PGOOD / IMU_INT (input)`, and make sure no expansion board drives it.

### How SPARE_IO reads after the rework

| Line | Meaning |
|---|---|
| High | No USB input, no fall |
| Low, IMU `WAKE_UP_SRC.FF_IA` set | Free fall |
| Low otherwise | USB or adapter present |

While the unit is plugged in, PGOOD holds the line low, so a fall can't wake it from deep sleep. Awake, the firmware reads the IMU directly. Plugging in also wakes the base from deep sleep, which is useful for showing charge state.

### Firmware already done

- The expander's first write switches the LED pins to current-sink mode (dim 0) before anything else, to shorten the power-on window on boards without R50 to R52.
- The expander interrupt now covers `SPARE_IO`.
- LSM6DSO INT2 is set to open-drain, active low, with free fall (312 mg, about 230 ms) routed to it (MD2_CFG).
- `usb_present()` reads PGOOD and ignores the line while the IMU reports a fall. On C6b the line reads high, so the code falls back to `CHG_N` as before.

### Bring-up checks for C7
1. Power a blank board from USB with no firmware. The LED may glow, but dimly and evenly; measure under 15 mA on the red channel.
2. Plug and unplug USB while the unit is running. The battery glyph should change within 2 s even when the cell is full.
3. Drop the unit about 30 cm onto a cushion while it's in deep sleep. It should wake.

## Applied to the routed board (rev C7)

`SignalScout_Base/SignalScout_Base.kicad_pcb` is now the routed v6 board with the rework applied, title block rev C.7.

| Change | Where |
|---|---|
| R50 82 Ω, R51 22 Ω, R52 22 Ω in series with LED1 red, green and blue cathodes (LED side nets `LED_RK_D`, `LED_GK_D`, `LED_BK_D`) | F.Cu, next to LED1, courtyards clear of LED1 and H4 |
| R53 1 kΩ, U6 pin 9 (`IMU_INT`) to `SPARE_IO` | F.Cu at (126.46, 120.5) |
| U10 pin 7 (PGOOD) to `SPARE_IO` | B.Cu out of the pin, then In2 to the existing `SPARE_IO` run |
| `CHG_N` rerouted around U10 (via, short In2 hop) | It blocked the only exit from pin 7 |
| Removed two vias that sat in pads and connected nothing | U1 pin 20 (`LCD_SCK`), C15 pad 1 (`DISP_VCIEN`) |

Checked here with an exact-geometry clearance check against the `.kicad_pro` net-class clearances: 0 violations, all 89 nets connected, no new courtyard overlaps, the +3V3 plane on L3 stays in one piece. Zone fills were stripped because they are stale: **press B to refill, then run KiCad DRC** before plotting.

### Still in pads (fill them)
Six vias in E22 pins 13 to 18 and one in U1 pin 21 (`LCD_D1`) cannot move: above the E22 pads is the SMA no-via zone and the 50 Ω `RF_LORA` line, below is the module keep-out, and U1 pin 21 has no free via site. The rest are thermal-pad and test-point vias. Order with **resin-filled, copper-capped vias (all vias)**.

### Solder mask bridges under 0.19 mm
U6 (LGA-14, 0.15 mm between pads) and U10 corner pins (0.177 mm) use the standard footprints. Let the fab open the mask there.

### ESP32 module (U1) DRC fixes
- **Ground plane under the antenna removed.** U1's antenna keep-out covered F.Cu, B.Cu and In2 but not In1, so the L2 GND plane filled under the PCB antenna (about 100 mm²). That detunes the antenna and cuts Wi-Fi/BLE range. The keep-out now includes In1.
- **H2 notch.** The keep-out is cut back to end at y = 176.3 for x < 146, which takes mounting hole H2 out of it (the hole sits 5.6 mm below the antenna end). **Use a nylon M2 screw or standoff in H2**; a metal one this close detunes the antenna.
- **Courtyard.** U1's body courtyard margin went from 0.75 mm (library) to 0.19 mm. The module body is 18 mm and sits in an 18.45 mm gap between the battery area and the USB-C (J2), so the library margin produced false overlaps with both. Physical clearances: 0.2 mm to the battery area, 0.25 mm to J2's courtyard.

The schematic in this folder is still rev C.4 and the PCB footprints carry no schematic links, so the PCB is the source of truth for C7.
