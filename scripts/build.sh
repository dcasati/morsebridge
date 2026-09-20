#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cli="${ARDUINO_CLI:-arduino-cli}"
fqbn='esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=enabled,DebugLevel=none,EraseFlash=none'
if ! "$cli" core list | grep -Eq '^esp32:esp32[[:space:]]+3\.3\.8([[:space:]]|$)'; then
  echo 'Required core: esp32:esp32@3.3.8 (see README installation command)' >&2
  exit 1
fi
mkdir -p build/esp32s3
"$cli" compile --fqbn "$fqbn" --warnings all \
  --build-path "$PWD/build/esp32s3" MorsePaddle
printf '\nBuilt %s/build/esp32s3/MorsePaddle.ino.bin\nFQBN: %s\n' "$PWD" "$fqbn"
