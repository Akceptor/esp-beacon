# Receiver contract

How to build the LoRa receiver that logs beacon chip IDs + RSSI, and how to
triangulate later. The beacon side is `src/main.cpp` (this repo). A receiver must
match every parameter below or it will never hear the beacons.

## Radio (LR1121 — must match exactly)

| Param | Value |
|-------|-------|
| Frequency | 868.0 MHz |
| Spreading factor | 12 |
| Bandwidth | 125 kHz |
| Coding rate | 4/5 |
| Sync word | 0x34 (private — NOT LoRaWAN's 0x12) |
| Preamble | 8 |
| CRC | ON |
| Beacon TX power | 13 dBm |

Same chip family on the receiver side: `new Module(CS, DIO9, RST, BUSY)` as an `LR1121`,
then `radio.begin(868.0, 125.0, 12, 5, 0x34, <power>, 8, <tcxo V>)` before
`radio.startReceive()`. The TCXO voltage and RF switch table (DIO5/DIO6) depend on the
receiver's board. Use the same values as the beacon's `src/main.cpp`.

The receiver's LoRa settings are what matter on air. An SX127x receiver with the same
SF/BW/CR/sync word may hear the beacon too, but this has not been tested.

## Packet (9 bytes, CRC on)

```
offset  bytes  field
0       2      magic  0xB2 0x7D
2       6      chip id  = beacon's ESP32-C3 Wi-Fi base MAC (unique per beacon)
8       1      tx power in dBm  (= 13)
```

The LoRa CRC covers the payload, so a received frame that passes the radio CRC is
trusted as-is. Discard frames whose first two bytes are not `B2 7D`, or whose CRC
failed (`RADIOLIB_ERR_CRC_MISMATCH` on receive).

## Per-receive log record

On a valid frame, store:

```
{ timestamp, chipId(6 bytes / MAC), rssi(int8, dBm), txPower(int8, dBm) }
```

- `timestamp` — receiver's own wall/clock time (monotonic + real-time if you can).
- `rssi` — `radio.getRSSI()` immediately after a successful receive.
- `txPower` — the byte at offset 8 (should be 13); keep it per-record so it survives
  future changes to beacon power.

Dedup: beacons send 2×/sec, so the same chip id repeats. Key the log by
`(chipId, timestamp)` and drop near-duplicate timestamps if you want one reading per
beacon per second rather than two.

## RSSI → distance

LoRa RSSI is noisy; treat a single reading as a range estimate, not a fix. Convert
RSSI to distance with a calibrated path-loss model (log-distance, e.g.):

```
d = 10 ^ ( ( txPower − rssi − A ) / (10 · n) )
```

- `txPower` — the beacon's output power (the packet byte, 13 dBm).
- `rssi` — measured, dBm.
- `A` — reference path loss at 1 m (measured/assumed, dBm).
- `n` — path-loss exponent (≈2 free-space; higher indoors, walls, ~2–4).

**A and n must be calibrated** against your environment — measure at known distances
and fit them. Different `n` per environment; indoor with walls typically needs a
higher `n`. Average several readings (from the 2×/sec cadence) before using one.

## Trilateration / positioning

With the same chip id observed at **≥3 receivers**, each gives a distance estimate
to the beacon (via the formula above). Trilaterate those 3+ ranges to a 2D (or 3D)
position of the beacon. Practically:

- Solve iteratively (RSSI ranges are noisy → no closed-form clean solution);
  least-squares or a coarse grid search over candidate positions, picking the point
  that minimizes the residual between predicted and measured ranges.
- More receivers improve the fix; weight each range by its RSSI reliability
  (stronger, steadier signal → more weight).
- A moving beacon: track the position series over time, not a single frame.

## Notes / caveats

- **Duty cycle (informational).** At SF12, 2 Hz, the beacon's air time is tens of
  ms per 500 ms — a high duty cycle. Fine for this indoor lab; **illegal for EU 868
  MHz outdoor use** (1% duty limit). For real deployment drop SF or move bands.
- **Sync word 0x34** keeps beacons off LoRaWAN traffic and lets you share the band
  with other private 0x34 systems.
- **Receiver clock**: keep it accurate (NTP) if you want to fuse RSSI from multiple
  receivers into one position — the timestamps must be comparable.
