# LoRa mesh

- Radio: RadioLib driving the SX1262, 500 kHz bandwidth, SF9, 902 to 928 MHz.
- Routing: managed flooding with a hop limit, duplicate suppression and a random relay delay.
- Priority: SOS messages take priority.
- Airtime: a self-imposed 10 % budget (SOS may exceed it).
- Peer to peer, no subscription. Messages are canned quick replies.

The logic is radio-independent and unit-tested (`firmware/lib/ss_core/mesh.*`).
