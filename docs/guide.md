# MorseBridge setup and firmware reference

[Back to the overview](../README.md)

Detailed instructions for building, configuring and troubleshooting MorseBridge.
Run all shell commands from the **repository root**, not from `docs/`.

- [Hardware and wiring](#hardware-and-wiring)
- [Build](#build)
- [Upload and BOOT recovery](#upload-and-boot-recovery)
- [BLE-only rollback](#ble-only-rollback)
- [iPhone and Morse-it](#iphone-and-morse-it)
- [Diagnostics and behavior](#diagnostics-and-behavior)
- [Enclosure prototype](#enclosure-prototype)

## Validation record

The previous BLE-only build
(`c055d0d`) was flashed and used successfully with paddles in Morse-it.
The dual-transport build passes host tests and the pinned ESP32-S3 build.
Firmware `70bdd09` was flashed on the S3 on 2026-09-22 without erasing NVS.
macOS recognizes **MorseBridge as a USB HID keyboard** with diagnostic CDC.
Live diagnostics show USB configured, awake and armed, a queued neutral report,
and the existing bonded BLE connection retained with BLE paddle reports disabled.
Application compatibility is [user-confirmed in the overview](../README.md#known-working-apps). Exact transport/platform
combinations and physical handoffs remain to be recorded; enumeration alone
does not establish application compatibility.

## Hardware and wiring

Target: [Waveshare ESP32-S3-Zero](https://www.waveshare.com/wiki/ESP32-S3-Zero),
ESP32-S3FH4R2, **4 MB flash, 2 MB QSPI PSRAM**, native USB-C.

| Passive 3.5 mm TRS jack | Board connection | Function |
| --- | --- | --- |
| Tip | GPIO4 | Dit / Left Control |
| Ring | GPIO5 | Dah / Right Control |
| Sleeve | GND | Common return |

Both contacts are configured as `INPUT_PULLUP`: a contact is pressed when it
closes to GND. Verify the actual paddle plug and jack lug mapping with a
continuity meter before connecting; some paddles reverse tip/ring. Swap those
two leads if needed. The numbers above are **GPIO numbers**, not physical pin
positions.

GPIO4 and GPIO5 are exposed, non-strapping pins, separate from native USB
GPIO19/20, flash/PSRAM and the onboard GPIO21 WS2812. Leave GPIO19/20 alone.
For noisy or longer wiring, optional external **10 kOhm pullups from each input
to 3.3 V** can supplement the internal pullups. Keep wiring short.
**Never connect 5 V, transmitter/keying voltage, or a powered keyer output to
these GPIOs.** This input is for isolated passive contacts only, not a
transmitter keying interface.

Power the board through USB-C from a computer, USB supply or power bank. Boot
does not wait for Serial or a USB host. Some power banks shut off with a
low-current load; use an always-on/low-current mode or an appropriate supply.
No battery measurement or charging circuit is implemented. The HID battery
service reports a fixed 100% for this USB-powered device.

## Build

Use **Arduino ESP32 core 3.3.8**, which includes the BLE library. Do not install
a third-party `ESP32 BLE Keyboard` or `NimBLE-Arduino` library for this sketch.
The build script checks the installed core version and does not install or
upgrade dependencies automatically.

```sh
arduino-cli core update-index \
  --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.8 \
  --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
bash scripts/test.sh
bash scripts/build.sh
```

The host tests require a C++17 compiler (`c++`, or set `CXX`). Set `ARDUINO_CLI`
if the CLI is not on PATH. Standard `ARDUINO_DIRECTORIES_DATA` and
`ARDUINO_DIRECTORIES_USER` environment variables can isolate the Arduino
installation from other projects.

Exact tested FQBN (also used by `scripts/build.sh`):

```sh
FQBN='esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=enabled,DebugLevel=none,EraseFlash=none'
arduino-cli compile --fqbn "$FQBN" --warnings all \
  --build-path "$PWD/build/esp32s3" MorseBridge
```

Equivalent Arduino IDE selections:

| Setting | Value |
| --- | --- |
| Board | ESP32S3 Dev Module |
| ESP32 package version | 3.3.8 |
| USB Mode | USB-OTG (TinyUSB) |
| USB CDC On Boot | Disabled (firmware starts diagnostic CDC manually) |
| USB Firmware MSC / DFU On Boot | Disabled |
| Upload Mode | UART0 / Hardware CDC |
| CPU Frequency | 240 MHz |
| Flash Mode / Size | QIO 80 MHz / 4 MB |
| Partition Scheme | Huge APP (3 MB No OTA / 1 MB SPIFFS) |
| PSRAM | QSPI PSRAM |
| Core Debug Level | None |
| Erase All Flash Before Sketch Upload | Disabled (normally) |

The large app partition leaves ample room; the filesystem partition is unused
and no OTA updater is implemented. Build output is ignored by Git under
`build/esp32s3/`, including `MorseBridge.ino.bin`, its ELF, bootloader, partition
table and merged image. Use the CLI upload command below so all images go to
their correct offsets, rather than flashing the app binary at address zero.
HID and diagnostic CDC are registered before `USB.begin()`: do not enable CDC
on boot or change to Hardware CDC/JTAG. The sketch rejects those settings.
The prototype retains Arduino's default Espressif USB VID/PID; these are not a
commercial VID/PID assignment for MorseBridge.

**S3 backend note:** core 3.3.8 builds ESP32-S3 with **NimBLE**, whereas the
original ESP32 transport used Bluedroid. This project uses the core's unified
`BLE*` APIs with native NimBLE connection/security/subscription callbacks,
synchronous advertising-start results and automatically managed CCCDs. It
does not fake a Bluedroid compile flag or use a manually added `BLE2902`.
The version is pinned because BLE APIs and backend defaults differ across
core versions.

## Upload and BOOT recovery

Use a known **USB data cable**. Once the board is visible, find its actual port:

```sh
arduino-cli board list
```

With `FQBN` set exactly as above, replace the example port with the detected one:

```sh
PORT='/dev/cu.usbmodem...'
arduino-cli upload --fqbn "$FQBN" --port "$PORT" \
  --input-dir "$PWD/build/esp32s3" MorseBridge
```

If the board is not detected or the previous sketch prevents USB enumeration,
hold **BOOT**, tap and release **RESET**, then release BOOT. Alternatively hold
BOOT while plugging in USB-C. Select the newly enumerated port and upload.
After upload, tap RESET if it remains in the ROM download loader. The serial
port name can change between ROM download mode and the running firmware.
If no USB device appears even in BOOT mode, check the cable, port and board
power first; software cannot flash a device that the host does not enumerate.

Normal uploads preserve Bluetooth bonds in NVS. For persistent stale-bond
problems, **forget the device on the iPhone and deliberately erase board flash
before reuploading**: in Arduino IDE enable *Erase All Flash Before Sketch
Upload* for one upload, then restore Disabled. This removes **all** board data,
including bonds, not just the application. No erase is performed by the build
script, and there is no bond-reset button gesture or serial command.

### BLE-only rollback

The known BLE-only version is commit `c055d0d675da2caa005ef69cd9abcd4ecbf71870`.
Build it separately without switching branches or disturbing current edits:

```sh
mkdir -p build/ble-rollback
git archive c055d0d675da2caa005ef69cd9abcd4ecbf71870 MorseBridge scripts/build.sh \
  | tar -x -C build/ble-rollback
(cd build/ble-rollback && bash scripts/build.sh)
```

Enter BOOT mode as described above, select its port and upload the rollback
images using the **old** USB configuration:

```sh
BLE_FQBN='esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app,PSRAM=enabled,DebugLevel=none,EraseFlash=none'
arduino-cli upload --fqbn "$BLE_FQBN" --port "$PORT" \
  --input-dir "$PWD/build/ble-rollback/build/esp32s3" build/ble-rollback/MorseBridge
```

## iPhone and Morse-it

1. **Wired:** connect the board directly to the unlocked iPhone 15 Pro Max with
   a USB-C **data** cable; Bluetooth pairing is not required. Expect cyan after
   releasing the paddles. Follow any iOS accessory-unlock prompt.
   **Wireless:** power from a charger/power bank, then pair with **MorseBridge**
   in iPhone **Settings > Bluetooth**. Expect green after release. Pairing uses
   bonded, encrypted JustWorks without MITM protection; pair in a trusted place.
2. In Morse-it, open **Settings > Hardware Interface > Iambic/Memory** and enable
   **Keyboard Enabled**. Set **Dot = Left Control**, **Dash = Right Control**.
3. Set **Key Type = Iambic A** or **Iambic B** to match your preference, and choose
   speed/sidetone settings in Morse-it.
4. Open **Tap** or **Start Sending Trainer**. In a keyboard event viewer, tap
   its text area to give it focus before trying the paddles.
5. Release **both** paddles after connection, route changes, resume or
   subscription changes to arm, then send normally.

### Automatic transport selection

USB is selected only when its host configures the device, not from cable power,
CDC DTR or an open serial monitor. Both transports may remain connected, but
paddle presses go to only one. During a BLE-to-USB handoff, BLE gets a neutral
report first; if it cannot accept the release, the firmware requests a BLE
disconnect and waits for that callback before enabling USB reports. Each newly
selected route sends neutral and waits for both contacts to be released.

**A Mac/PC data connection selects USB too**, even if you only wanted a serial
monitor. To use BLE with the iPhone, power the board from a charger/power bank
instead. A suspended USB host retains priority (amber) until resume or USB
unmount; the firmware does not send duplicate keys through BLE during sleep.
If the board loses power when unplugged, it restarts when powered again.
With independent power, USB unmount permits BLE fallback; without VBUS sensing,
loss of data wires alone may look like host suspension rather than unmount.

Left Control is modifier `0x01`, Right Control is `0x10`, and a squeeze is
`0x11`. Each report has exactly eight bytes: modifier, zero reserved byte, six
zero key usages. Releases send modifier `0x00`; holds remain held without
artificial repeats or tap conversion. These are modifier events, **not
printable text**, so an ordinary text field may show nothing.

If Bluetooth says connected but Morse-it does nothing, check keyboard mapping,
the active sending screen/focus, and that both paddles were released. Disconnect
other hosts that may reconnect to the board. If iOS has cached an earlier
keyboard profile, forget the old device and pair again. The distinct name
avoids confusing it with *Morse Tutor*, but changing the name alone does not
clear a cached identity/bond on the same physical board.

**Name migration:** iOS may keep displaying the former name, *Morse Paddle*,
for an existing bond after the firmware rename. Reconnect first; if the label
stays cached, forget that device on the iPhone and pair with **MorseBridge**.
The rename does not erase bonds or change the HID report format or GPIO mapping.

## Diagnostics and behavior

USB diagnostic CDC is 115200 baud, with **no boot wait or host-delivery wait**.
Opening a serial monitor can reset some boards/USB configurations and interrupt
BLE; reconnect and release both paddles afterward. Logging is best effort:
without a reading host, messages may be dropped rather than delay keying.
All firmware output uses one non-waiting writer guard and reserves a complete
line in a fixed 192-byte buffer, draining it in FIFO-sized chunks without
interleaving other lines. The pinned core's CDC TX FIFO is only 64 bytes.
Queuing directly to TinyUSB avoids both the old
HWCDC full-buffer stall and core 3.3.8's `USBCDC::write` behavior that drops all
writes with a zero timeout. No code waits for the host to consume diagnostics.
BLE callbacks never wait for another log writer. Dropped lifecycle/error
messages are counted and reported as `LOG: dropped ...` once space returns;
current GPIO/USB/BLE snapshots resume automatically.
There are boot messages, USB/BLE error messages and status transitions.
Paddle diagnostics are currently enabled (`MORSEBRIDGE_DEBUG=1` in
`MorseBridge/PaddleDebug.h`). Set that default to `0`, or compile with
`-DMORSEBRIDGE_DEBUG=0`, to disable the additional snapshots.
The former `MORSE_PADDLE_DEBUG` compile flag remains a compatibility alias.

### Checking the jack and paddle over USB

Open the board's serial port at **115200 baud**. No iPhone connection is needed
to see the input levels, even if BLE startup fails. Press/release each paddle,
then squeeze both. Expect:

| Contact state | GPIO4 | GPIO5 |
| --- | --- | --- |
| Both released | HIGH | HIGH |
| Dit held | LOW | HIGH |
| Dah held | HIGH | LOW |
| Both held | LOW | LOW |

Example snapshot:

```text
DBG GPIO4=LOW GPIO5=HIGH edges=1/0 C=1 A=1 S=1 U=0 R=0 Q=1 M=00 T=USB UC=1 US=0 UR=1 UQ=2 UM=01
```

`edges` counts **raw transitions** on GPIO4/GPIO5 since boot, including contact
bounce; a short tap can increase the counters even if the sampled display has
already returned to HIGH. Changes are summarized at most ten times per second,
with a heartbeat every two seconds. The logger never waits for USB buffer
space, and retains transition totals while there is no reading host.

The BLE flags are `C` connected, `A` encrypted and bonded, `S` HID notifications
subscribed, `U` host suspended, and `R` ready/armed. `Q` counts reports accepted
by the local BLE stack since boot, **not confirmed delivery to Morse-it**.
`M` is the last accepted report's modifier byte: `01` dit, `10` dah, `11` both,
`00` released. It is historical, so check the connection flags alongside it.
`T` is the selected route (`USB`, `BLE`, `NONE`); `UC` is USB configured,
`US` is USB suspended, `UR` is USB armed, `UQ` counts locally queued USB reports,
and `UM` is their last modifier byte. USB counters also do **not** confirm
delivery to Morse-it. On a working USB route, BLE `R=0` is expected even when
Bluetooth is still connected.

If neither input changes, check the **sleeve/common wire to GND**, the jack's
tip/ring lugs (not its switched lugs), and plug continuity with power unplugged.
If an input stays LOW with the paddle released, check for a short or incorrect
jack lug; the firmware cannot arm until both inputs are HIGH. If the input
levels change, check the selected route: USB needs `UC=1 US=0 UR=1`; BLE needs
`C/A/S/R=1 U=0`. If its report counters/modifiers change correctly, check the
host receiving those reports, Morse-it's keyboard mapping and event-viewer focus.

| Serial status | Meaning |
| --- | --- |
| Waiting | Waiting for USB configuration or BLE connection |
| Waiting for HID subscription, route release, or USB resume | Link exists but reports are not ready |
| Release BOTH paddles | Initial neutral report/released contacts required |
| BLE ready | Encrypted, bonded, subscribed and armed |
| USB ready | Configured, awake and armed; BLE paddle reports suppressed |
| ERROR | Read the preceding USB/BLE error, then reset/power-cycle |

### Status light

The onboard **GPIO21 RGB LED** lights at low brightness as soon as firmware starts:

| Color | Meaning |
| --- | --- |
| Blue | Powered, waiting for a USB host or BLE connection |
| Amber | Waiting for HID readiness, old-route release, USB resume or paddle release |
| Green | BLE ready and paddles armed |
| Cyan | USB ready and paddles armed |
| Red | Selected/available route initialization, pairing, advertising or report error |

The onboard LED uses **RGB byte order, not GRB**, as documented in the
[Waveshare FAQ](https://docs.waveshare.com/ESP32-S3-Zero/FAQ).
Using GRB swaps ready/green and error/red.

The color changes only with status, never on each paddle press. A persistent
WS2812 frame is sent using asynchronous RMT, so the paddle loop does not wait
for LED transmission. A transmit failure or 20 ms completion timeout logs an
error and disables further LED updates without stopping HID. The last color
can remain latched after such a failure; check Serial if its meaning is unclear.
The LED is a firmware indicator, not an independent power-good indicator: it
may remain off in the ROM bootloader or if startup/LED hardware fails.

The advertising watchdog reports an error only after advertising has been
continuously absent for two seconds without a connection. Normal advertising
shutdown just before a connection callback is not itself an error.

Contacts debounce independently for 5 ms on each edge. The loop yields for
1 ms; BLE/USB scheduling adds latency, so this is not a hard real-time keyer.
A new connection, selected route, resubscription or resume sends neutral before arming and
requires both contacts released. Security and subscription callbacks received
before the connection callback are retained; disconnect clears them for the
next session. Failed notifications retry at 10 ms intervals,
with three consecutive failures causing an explicit error and disconnect to
release keys. Normal disconnects restart advertising automatically. Initialization
and services are retained instead of allocating new services on reconnect.
Pairing failures and advertising failures latch an error rather than silently
pretending to be ready or repeatedly restarting.
USB uses the same eight-byte report and ID 1 (ID 0 in boot protocol).
USB reports queue without waiting for host completion. Busy endpoints retry
without resetting debounce; a pending report blocked for 250 ms, or three
consecutive queue failures at 10 ms intervals, latches an error and detaches
USB to release the host's keyboard state. Reset is required to restore a
faulted transport. A failure on an unused transport does not stop a healthy
selected one; consult diagnostic error messages.

The host tests cover debounce/bounce, rollover, hold/release/squeeze report bytes,
bond/encryption/subscription gating, reconnection, suspend/resume, neutral-send
failure, service reuse, bounded notification failures, advertising failures and
the actual sketch's host-independent startup/GPIO mapping/status logging.
Dual-route cases cover USB-only, BLE-only/power-only, both hosts, held-paddle
handoffs, delayed BLE disconnects, failed releases, USB busy/failure/boot
protocol, unplug/replug, suspend/resume, and missing/full diagnostic CDC.
They simulate both APIs; they do not prove USB enumeration, iOS recognition,
over-the-air pairing or latency.

Before relying on this build, verify on the actual S3 board: first pairing,
individual holds/releases and squeeze in Morse-it, reconnect/power-cycle with
both paddles held, resume, and operation on USB power without a computer.
For the experimental wired mode, verify USB HID + CDC enumeration on a computer
without opening a monitor; direct USB-C/iPhone Morse-it input with Bluetooth
off; both transports connected with no double-keying; cyan versus green route
indication; handoffs while held; and recovery through BOOT mode. Do not interpret
successful USB enumeration alone as successful Morse-it input.

## Enclosure prototype

Download the **V6 fit-check prototype** without running the generator:
[base STL](../enclosure/v6/morsebridge_base_45x24x16_v6.stl) |
[lid STL](../enclosure/v6/morsebridge_lid_45x24x16_v6.stl) |
[mesh-check report](../enclosure/v6/mesh-checks.json).
On each STL's GitHub page, use **Download raw file** to save it for your slicer.
Print both V6 parts together and read the [fit notes](../enclosure/FIT-CHECK.txt)
first. These are nominal geometry checks, not a guarantee of physical fit.

The editable **MorseBridge** enclosure generator and fitting notes are in
[`enclosure/`](../enclosure/). Generate the current V6 pair with:

```sh
python3 -m venv .venv-enclosure
.venv-enclosure/bin/pip install -r enclosure/requirements.txt
.venv-enclosure/bin/python enclosure/prototype.py --output build/enclosure
```

Outputs are `morsebridge_base_45x24x16_v6.stl` and
`morsebridge_lid_45x24x16_v6.stl`: **45 x 24 x 16 mm assembled outside**,
41.8 mm internal length and 12.8 mm floor-to-lid clearance.
The USB-C opening is rounded 9.5 x 3.4 mm,
the jack opening is 6 mm diameter, and the LED opening is 3 mm diameter.
V6 keeps the four PCB pads and USB-end guides removed, retaining the jack cradle.
The lid uses two flexible snap clips with catches and side release slots, plus
a shorter locating lip with 0.4 mm clearance per side. Print both V6 parts;
earlier lids are not compatible. The prototype was printed in PLA+; PETG is
also an option. Dry-fit the empty case before installing electronics.
The LED hole centre is moved 1 mm toward USB-C, to 13.5 mm from that end.
USB opening height is unchanged; the PCB needs separate insulated retention.
Read `enclosure/FIT-CHECK.txt` before printing: physical fit, LED alignment,
and final mechanical retention are not qualified.

## Provenance and license

`BlePaddleKeyboard.{h,cpp}`, `PaddleInput.h`, and their original host tests/fakes
were adapted from
[`dcasati/morse-tutor` at `01b5c93f9198a7b5ae5febc1caab8cd6b04c2255`](https://github.com/dcasati/morse-tutor/commit/01b5c93f9198a7b5ae5febc1caab8cd6b04c2255).
This separate project adds the S3/headless entry point, core NimBLE adaptation,
stricter neutral/subscription/resume handling, additional tests and board-specific
instructions. It does not modify the original tutor.

Distributed under the [MIT license](../LICENSE), retaining the upstream
**Copyright (c) 2019 Bruce E. Hall** notice; the original tutor is not represented
as newly authored here. Arduino-ESP32 and its bundled BLE/ESP-IDF components
retain their own licenses and are installed separately, not vendored.
