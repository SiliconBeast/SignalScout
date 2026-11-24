# Known limitations

- Nothing has run on the real boards yet.
- The display driver assumes a CO5300 controller at 410 x 502 with a 22-column offset. Confirm against the panel datasheet.
- The Sweep detector is broadband and uncalibrated until the constants in the sled sketch are set against a known source.
- The navigation arrow uses GPS course over ground, so it only settles while moving. There is no magnetometer.
- Messages are canned quick replies; there is no keyboard.
- Touch assumes an FT3168 or CST816 class controller with axes matching the panel.
- Over-the-air updates need a published release.
- LoRa settings target the 902 to 928 MHz band (Canada/US).
