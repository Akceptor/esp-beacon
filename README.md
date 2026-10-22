# esp-beacon

Indoor-lab LoRa beacon for ESP32-C3 + LR1121. Every 500 ms (2 Hz) it transmits a
9-byte packet carrying its chip ID and TX power so a separate LoRa receiver can
tell beacons apart and (later) triangulate them by RSSI.

The beacon is one of a fleet; each gets a unique chip ID from the ESP32-C3's burned-in
Wi-Fi base MAC — no provisioning. See **`docs/RECEIVER.md`** for the full contract a
receiver must match.

## Packet (9 bytes, radio CRC on)

```
0  B2 7D      magic
2  6 bytes    chip id = ESP32-C3 Wi-Fi base MAC
8  1 byte     TX power in dBm (13)
```

## Radio (LR1121)

868.0 MHz · SF12 · 125 kHz · CR 4/5 · sync word 0x34 · preamble 8 · TX 13 dBm (HP PA) · CRC on · crystal (no TCXO).
All values live in `include/board.h`, shared by the beacon and the receiver (kept in sync
with `platformio.ini` and `docs/RECEIVER.md`).

## Pin map (ESP32-C3 GPIO)

From the ExpressLRS `UNIFIED_ESP32C3_LR1121_RX` layout (via the Meshtastic variant).

| Signal | Pin |
|--------|-----|
| SPI SCK  | 6 |
| SPI MISO | 5 |
| SPI MOSI | 4 |
| SPI CS   | 7 |
| RST      | 2 |
| BUSY     | 3 |
| DIO9/IRQ | 1 |
| RF switch | DIO5–8 (on the LR1121; RX=DIO7, TX=DIO8) |
| LED      | 8 (single NeoPixel) |

ExpressLRS leaves its own firmware on the LR1121, which stock RadioLib rejects.
`extra_scripts/lr11x0_accept_trx_firmware.py` patches the downloaded RadioLib at build time
(RadioLib is pinned to 7.8.1 so the patch keeps matching).

## Web flasher

**https://akceptor.github.io/esp-beacon/** — pick Beacon or Receiver and flash an ESP32-C3
from Chrome/Edge, no tools needed. Put the board in bootloader mode first (hold BOOT, tap
RST, release BOOT), click Connect, then Flash, then tap RST to run it.

The page lives in `flasher/` and is deployed by `.github/workflows/pages.yml`. The images
under `flasher/firmware/` are generated: after changing anything in `src/` or `include/`, run

```
flasher/build.sh
```

and commit the updated `.bin` files and `manifest.json`. To try the page locally:
`cd flasher && python3 -m http.server`, then open http://localhost:8000.

## Build / flash

Needs [PlatformIO](https://platformio.org). The platform is pinned to `espressif32@6.12.0`
in `platformio.ini`.

```
pio run -e beacon -t upload --upload-port /dev/cu.usbserial-0001
pio device monitor -e beacon
```

This board has no auto-reset: hold **BOOT**, tap **RST**, release BOOT to enter the
bootloader before flashing, and reset it after to run. At boot it prints the chip ID +
radio params, then `beat N` about once a second. The NeoPixel blinks green per transmit.

## Receiver (second board)

`src/receiver.cpp` listens with the same radio config and prints one CSV line per valid
packet (`chipId,rssi_dBm,snr_dB,txPower_dBm`), plus a `# listening` status every 10 s.

```
pio run -e receiver -t upload --upload-port <port-of-second-board>
pio device monitor -e receiver
```

The NeoPixel blinks blue per valid packet. Use it to confirm a beacon is audible; the
full contract for a real receiver is in `docs/RECEIVER.md`.

## Power / battery

The MCU idles (`delay`) in the 500 ms gaps; the LR1121 sits in standby between packets.
This is deliberately simple — no fixed-duration light sleep, because deep sleep would
re-init the radio and break the 500 ms cadence. For real battery life, gate the radio
power and power it on only during transmit. That's a later optimization.

## Notes

- **Indoor lab only.** At SF12 / 2 Hz the duty cycle is high — fine here, illegal for
  EU 868 MHz outdoor use.
- Receiver is **not** included; `docs/RECEIVER.md` is the contract to build it against.
