# FAQ

**Does it work on the real hardware yet?** Not yet. The logic, UI and protocol are tested on the host and the firmware builds in CI. See [BRINGUP.md](BRINGUP.md) and [TODO.md](TODO.md).

**Can Sweep tell me what is transmitting?** No. The AD8317 detector is broadband. Sweep reports how much energy is present and whether the visible Wi-Fi/BLE emitters explain it.

**Does it need a subscription?** No. Messages go peer to peer over the LoRa mesh.

**Can it jam or deauthenticate?** No. The firmware only receives and analyses, plus its own LoRa messages.

**Can I try the UI without hardware?** Yes: run the browser build in `web/` or the desktop simulator (`pio run -e sim`).

**Which LoRa band does it use?** 902 to 928 MHz (Canada/US). Check local rules before using it elsewhere.
