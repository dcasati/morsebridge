# Flash a firmware release

For the **Waveshare ESP32-S3-Zero with 4 MB flash and 2 MB PSRAM** only.
This is not firmware for the original ESP32-WROOM, a XIAO board, or the
ESP32-S3-Zero-N8R8.

## 1. Download

From the [latest release](https://github.com/dcasati/morsebridge/releases/latest),
download the firmware ZIP and `SHA256SUMS.txt`. Use the firmware ZIP, **not**
GitHub's automatically generated "Source code" archives.

Verify the ZIP before extracting it. On macOS:

```sh
shasum -a 256 -c SHA256SUMS.txt
```

On Linux, use `sha256sum -c SHA256SUMS.txt`. On Windows, use
`Get-FileHash .\morsebridge-v0.1.0-esp32s3-zero.zip -Algorithm SHA256` in
PowerShell and compare the result with `SHA256SUMS.txt`.

Extract the ZIP. It contains:

| File | Flash address |
| --- | --- |
| `bootloader.bin` | `0x0` |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xe000` |
| `firmware.bin` | `0x10000` |

`FLASHING.md`, `build-details.json`, `LICENSE` and a separate `SHA256SUMS.txt`
for the extracted files are included. **Do not flash `firmware.bin` at `0x0`.**

## 2. Install the flashing tool

Install Python 3 if needed, then create a virtual environment.

macOS / Linux:

```sh
python3 -m venv .venv
source .venv/bin/activate
```

Windows PowerShell:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
```

In the activated environment:

```sh
python -m pip install esptool==5.2.0
```

Arduino IDE and the ESP32 board package are not needed for this method.

## 3. Enter download mode and flash

Use a **USB data cable** and close serial monitors. Hold **BOOT**, tap and
release **RESET**, then release BOOT. Alternatively, hold BOOT while plugging
the board into USB.

Find the board's serial port: typically `/dev/cu.usbmodem...` on macOS,
`/dev/ttyACM0` on Linux, or `COM3` on Windows. The name can change in BOOT mode.
Unplug/replug to distinguish it from other serial devices.

In a terminal with the virtual environment active, change into the extracted
firmware folder. Replace `PORT` below with the board's actual port, then run
this command as **one line**:

```sh
python -m esptool --chip esp32s3 --port PORT --baud 115200 write-flash 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

Wait for verification to finish, then tap **RESET** if the board remains in
download mode.

This command writes four separate regions; it does **not** run `erase-flash`
or overwrite the NVS region at `0x9000`-`0xdfff`. Existing MorseBridge Bluetooth
bonds are preserved when updating from its current partition layout. Back up
any important data before replacing unrelated firmware; its partition layout
may differ. No merged whole-flash image is included.

## 4. Start using MorseBridge

Release both paddles after connection. **Cyan** means USB ready; **green**
means Bluetooth ready. Configure the app for **dit = Left Control** and
**dah = Right Control**.

A Mac/PC USB data connection selects wired input. To use Bluetooth with your
phone, power the board from a charger or power bank instead.

For troubleshooting, source builds or BLE-only rollback, see the
[setup and firmware guide](https://github.com/dcasati/morsebridge/blob/main/docs/guide.md).
