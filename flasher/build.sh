#!/usr/bin/env bash
# Builds every firmware and writes the flash images the web flasher uses, plus
# flasher/firmware/manifest.json (per hardware, per firmware: file, size, sha256, flash offset).
#   esp32c3: bootloader + partitions + boot_app0 + app merged into one image, written at 0x0.
#   esp8266: PlatformIO's firmware.bin is already a complete image for 0x0; copied as-is.
# Run after any change under src/ or include/, then commit the .bin files.
set -euo pipefail
cd "$(dirname "$0")/.."

ESPTOOL=${ESPTOOL:-$HOME/.platformio/penv/bin/esptool}
BOOT_APP0=${BOOT_APP0:-$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}
OUT=flasher/firmware
mkdir -p "$OUT"

for fw in beacon receiver; do
  pio run -e "$fw"
  b=".pio/build/$fw"
  "$ESPTOOL" --chip esp32c3 merge_bin -o "$OUT/esp32c3-$fw.bin" \
    --flash_mode dio --flash_freq 80m --flash_size 4MB \
    0x0 "$b/bootloader.bin" 0x8000 "$b/partitions.bin" \
    0xe000 "$BOOT_APP0" 0x10000 "$b/firmware.bin"

  pio run -e "${fw}_8266"
  cp ".pio/build/${fw}_8266/firmware.bin" "$OUT/esp8266-$fw.bin"
done

entry() { # hw fw flashMode
  local f="$OUT/$1-$2.bin"
  printf '    "%s": {"file": "firmware/%s-%s.bin", "offset": 0, "flashMode": "%s", "size": %s, "sha256": "%s"}' \
    "$2" "$1" "$2" "$3" "$(wc -c < "$f" | tr -d ' ')" "$(shasum -a 256 "$f" | cut -d' ' -f1)"
}
{
  echo '{'
  echo '  "esp32c3": {'
  entry esp32c3 beacon keep; echo ','
  entry esp32c3 receiver keep; echo
  echo '  },'
  echo '  "esp8266": {'
  entry esp8266 beacon dout; echo ','
  entry esp8266 receiver dout; echo
  echo '  }'
  echo '}'
} > "$OUT/manifest.json"
cat "$OUT/manifest.json"
