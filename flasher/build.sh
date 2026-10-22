#!/usr/bin/env bash
# Builds both firmwares and merges each into one flash image (bootloader + partitions +
# boot_app0 + app) at flasher/firmware/<env>.bin, which the web flasher writes at 0x0.
# Run after any change under src/ or include/, then commit the .bin files.
set -euo pipefail
cd "$(dirname "$0")/.."

ESPTOOL=${ESPTOOL:-$HOME/.platformio/penv/bin/esptool}
BOOT_APP0=${BOOT_APP0:-$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}

mkdir -p flasher/firmware
for env in beacon receiver; do
  pio run -e "$env"
  b=".pio/build/$env"
  "$ESPTOOL" --chip esp32c3 merge_bin -o "flasher/firmware/$env.bin" \
    --flash_mode dio --flash_freq 80m --flash_size 4MB \
    0x0 "$b/bootloader.bin" 0x8000 "$b/partitions.bin" \
    0xe000 "$BOOT_APP0" 0x10000 "$b/firmware.bin"
done

{
  echo '{'
  first=1
  for env in beacon receiver; do
    f="flasher/firmware/$env.bin"
    [ $first -eq 1 ] || echo ','
    first=0
    printf '  "%s": {"file": "firmware/%s.bin", "size": %s, "sha256": "%s"}' \
      "$env" "$env" "$(wc -c < "$f" | tr -d ' ')" "$(shasum -a 256 "$f" | cut -d' ' -f1)"
  done
  echo
  echo '}'
} > flasher/firmware/manifest.json
cat flasher/firmware/manifest.json
