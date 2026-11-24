# Over-the-air updates

The Updates app checks GitHub releases. A release publishes `firmware.bin` and `manifest.json` (see [RELEASING.md](RELEASING.md)).

The download goes to the spare flash slot and is verified with SHA-256. The bootloader rolls back automatically if the new version fails. Until a release is published, the Updates app reports that it couldn't check.
