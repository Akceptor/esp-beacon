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

**https://akceptor.github.io/esp-beacon/** — pick your hardware (ESP32-C3 + LR1121 or
ESP8266 + SX1276), then Beacon or Receiver, and flash from Chrome/Edge, no tools needed.
The page checks that the detected chip matches the selected hardware and refuses to flash
otherwise. ESP32-C3: put the board in bootloader mode first (hold BOOT, tap RST, release
BOOT). ESP8266/ESP8285: hold FLASH (GPIO0), tap RST, release (the reference module has no
working auto-reset). Then click Connect, Flash, and tap RST to run it.

The page lives in `flasher/` and is deployed by `.github/workflows/pages.yml`. The images
under `flasher/firmware/` are generated: after changing anything in `src/` or `include/`, run

```
flasher/build.sh
```

(it builds all four envs) and commit the updated `.bin` files and `manifest.json`. To try the page locally:
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

| Field | Meaning |
|-------|---------|
| `chipId` | Beacon's Wi-Fi MAC, unique per beacon |
| `rssi_dBm` | Received signal strength; closer to 0 is stronger (≈ −30 very close, ≈ −120 limit). Noisy: average several |
| `snr_dB` | Signal above noise; SF12 decodes down to ≈ −20 |
| `txPower_dBm` | Power the beacon says it used (13), read from the packet |

```
pio run -e receiver -t upload --upload-port <port-of-second-board>
pio device monitor -e receiver
```

The NeoPixel blinks blue per valid packet. Use it to confirm a beacon is audible; the
full contract for a real receiver is in `docs/RECEIVER.md`.

## ESP8266 + SX1276 variant

A generic ESP8285 (1 MB flash, DOUT mode; the ESP8266 core family) LoRa module with an
SX1276 (the **868 MHz** variant, 3.3 V supply only) runs the same beacon and receiver. Modulation, sync word and packet format match the
LR1121 build, so either kind of beacon is heard by either kind of receiver. The chip ID is
the chip's station MAC.

| Signal | GPIO |
|--------|------|
| SPI SCK  | 14 |
| SPI MISO | 12 |
| SPI MOSI | 13 |
| SPI CS (NSS) | 15 |
| RST      | 2 |
| DIO0/IRQ | 4 |
| DIO1     | 5 |
| Button   | 0 (unused) |
| LED      | 16 (plain LED, active low) |

GPIO0/2/15 are boot-strap pins. The antenna is wired to the SX1276 RFO pin (not
PA_BOOST), so the firmware forces RFO output; 13 dBm is in RFO's range.

Build with `pio run -e beacon_8266` (or `receiver_8266`). Don't use `-t upload`: PlatformIO's
bundled esptool 3.0 fails to sync with this chip. Put the board in the bootloader by hand
(hold FLASH/GPIO0, tap RST, release) — this module has no working auto-reset — then flash
with esptool 4.x:

```
esptool --chip esp8266 --port <port> --before no_reset --after hard_reset \
  write_flash --flash_mode dout --flash_size detect 0x0 .pio/build/beacon_8266/firmware.bin
```

Then tap RST; the serial monitor shows `beat N` about once a second. The web flasher also
flashes this variant (select ESP8266 + SX1276), with the same manual bootloader entry.

## Power / battery

The MCU idles (`delay`) in the 500 ms gaps; the LR1121 sits in standby between packets.
This is deliberately simple — no fixed-duration light sleep, because deep sleep would
re-init the radio and break the 500 ms cadence. For real battery life, gate the radio
power and power it on only during transmit. That's a later optimization.

## Notes

- **Indoor lab only.** At SF12 / 2 Hz the duty cycle is high — fine here, illegal for
  EU 868 MHz outdoor use.
- Receiver is **not** included; `docs/RECEIVER.md` is the contract to build it against.
