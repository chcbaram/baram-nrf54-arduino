# Adafruit example compatibility

*[English](EXAMPLE-COMPAT.md) · [한국어](EXAMPLE-COMPAT.ko.md)*

The result of compiling **all 71** of `adafruit/Adafruit_nRF52_Arduino`'s `Bluefruit52Lib/examples`
with this core. It is measured **so that what to build next is not decided by feel.**

⚠ **The denominator is not trimmed in advance.** All 71 are counted, and one is "removed from the count"
only after checking it individually. Once 16 were removed in bulk and then restored — at that time
three APIs, `Bluefruit.configUuid` · `getAddr` · `configAttrTableSize`, **existed only in the removed
examples and silently dropped out of sight.** The removal criteria are recorded in §Removal rules below.

⚠ **If an external library is needed, install it and actually measure.** "Cannot measure because the library
is missing" and "our API is missing" are completely different, and without measuring you cannot tell them
apart. After installing, `blemidi` turned out not to be a library problem but **us lacking `BLEMidi`**, and
`neopixel` was **our core lacking `interrupts()`/`noInterrupts()`**.

Last measured: 2026-09-12 · on XIAO nRF54L15 (`blemidi` · `client_cts` · `ancs` · `central_hid` added)

---

## Porting rule — delete three include lines

When measuring, only the three lines below are removed; nothing else is touched.

```cpp
#include <Adafruit_TinyUSB.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
```

**All three only include; none uses the API.** Checked exhaustively —
of the 23 that include TinyUSB, **0** use its API, and of the 6 that include the filesystem,
**0** use its API.

Arduino "links only the libraries a sketch includes", so Adafruit added those lines to pull in `Serial`
(USB CDC) and bond storage. In this core `Serial` is a UART and bonds go into an RRAM partition,
so neither is needed. Background: CLAUDE.md §8.1.

> ⚠ At first the 23 that include TinyUSB were classified as "structurally impossible since there is no USB".
> **That was wrong.** Basic examples unrelated to BLE such as `adc` · `hwpwm` · `rtos_scheduler` were
> lumped in there. R10 (no USB hardware) is true, but it was not what blocked these examples.

## Examples we ship (28)

**We do not make users install the nRF52 core just to get the upstream examples.** Claiming Adafruit
compatibility without shipping examples would amount to users installing another core only to take its
examples. So whatever works ships as an example.

```
Peripheral/  bleuart  bleuart_multi  custom_service  adv_advanced  rssi
             beacon  eddystone_url  pairing  clearbonds
             blehid_keyboard  blehid_button  blehid_mouse  blehid_media
             blehid_gamepad  throughput
Central/     central_scan  central_bleuart  central_client
DualRoles/   dual_bleuart
Hardware/    blinky  SerialEcho  temp_measure  rtos_scheduler  board_test
```

They build on all four boards (nu54dk / nu54vdk / xiao_nrf54l15 / xiao_nrf54lm20a).

⚠ The upstream examples were **rewritten**, not copied. This is to match the comment style and to note
in place what differs on this core (e.g. `temp_measure` has no asynchronous version yet).
`blehid_keyboard`, like upstream, types whatever arrives on serial into the host —
**it goes into the focused window**, so its comment carries a warning. `blehid_button` is not upstream;
it maps one key to one button so a test can be controlled.

`dual_bleuart` does peripheral and central **at the same time** —
`begin(1, 1)`. This is possible because role allocation happens at run time (see section B8).

## Upstream examples not shipped yet (57) — by reason

We ship 15 under matching names. The rest are grouped by **why** they are not (or cannot be) included.
"What to build first" goes A -> B -> C.

### A. Our API can do it now (only the example is missing) — 21

```
pairing_pin  pairing_passkey  central_pairing
rssi_callback  rssi_poll
temp_measure_blocking  temp_measure_non_blocking
adv_AdafruitColor  nrf_blinky  controller  image_transfer
custom_hrm  custom_htm
blehid_camerashutter  blehid_keyscan
central_bleuart_multi  central_scan_advanced  central_custom_hrm
rssi_proximity_central  rssi_proximity_peripheral
central_throughput
```

⚠ About half **are already covered by our examples** — `pairing` / `clearbonds` /
`rssi` / `temp_measure` / `custom_service` differ only in name. There is little reason to add one
more each under the upstream name.

`throughput` **now exists as our example.** Throughput was measured with it
(both directions 26–28 KB/s against a Mac, `docs/STATUS.md` §2.6), including that notify queue
depths 1/3/6 are all within measurement noise. The upstream `central_throughput` compiles but is not
shipped under our name yet — it needs two boards.

### B. Our BLE API is missing — 3

| Example | Status | Needed |
|---|---|---|
| `dfu_ota` | 📅 M4 | Real DFU. `BLEDfu` today only registers the service and clearly refuses |
| `dfu_serial` | 📅 M4 | Needs the bootloader |
| `blinky_ota` | 📅 M4 | Same |

### C. M2 (Arduino API) is missing — 12

All 📅 **planned** (M2). Unrelated to BLE, and the hardware is all there.

| Example | Needed |
|---|---|
| `adc` `adc_vbat` | `analogRead` (SAADC) |
| `Fading` `hwpwm` | `analogWrite` / PWM |
| `digital_interrupt_deferred` | `attachInterrupt` (GPIOTE) |
| `Serial1_test` | A second UART. ⚠ Its pins are in the P2 domain, so external wiring is needed |
| `hw_systick` `software_timer` | Timer API |
| `hwinfo` `fwinfo` `meminfo` | Information output — our `board_test` does something similar |
| `blink_sleep` | Low-power API (§4.5) |

### D. External devices and board-specific hardware — 16

**This is where "cannot work" and "not done yet" get mixed up.** Mixed together they would later make
us re-investigate "was this impossible from the start?", so each is separated.

| Example | Status | Reason |
|---|---|---|
| `neopixel` | 🚫 **permanently impossible** | Bit-banging. The SoftDevice owns the top interrupt priority and breaks the timing (§7 F6). **It does compile** — which makes it more dangerous |
| `neomatrix` | 🚫 **permanently impossible** | Same (NeoPixel based) |
| `gpstest_swuart` | 🚫 **permanently impossible** | Same (SoftwareSerial) |
| `tone_happy_birthday` | 🔧 hardware present | `tone()` not implemented. It can sit on top of PWM (M2). The buzzer is an external part |
| `nfc_to_gpio` | 🔧 hardware present | **The chip has NFCT** (both L15·L05, `NFCT_IRQn=214`, NFC1/2 = P1.02/03). Only the driver is missing |
| `ancs_oled` | 📦 external part | SSD1306 OLED. `Wire` (M2) first |
| `client_cts_oled` | 📦 external part | Same |
| `image_eink_transfer` | 📦 external part | e-ink panel. `SPI` (M2) first |
| `central_ti_sensortag_optical` | 📦 external device | Needs a real TI SensorTag. The core can already do it (`BLEClientService`) |
| `ancs_arcada` | 🧩 Adafruit board | `Adafruit_Arcada.h` — that board only. Replaceable by `ancs` (the non-Arcada version) |
| `pairing_passkey_arcada` | 🧩 Adafruit board | Same. Replaceable by `pairing_passkey` |
| `bluefruit_playground` | 🧩 Adafruit board | CPB/CLUE sensors + custom `BLEAdafruit*` services |
| `arduino_science_journal` | 🧩 Adafruit board | CircuitPlayground + APDS9960 |
| `tf4micro-motion-kit` | 🧩 Adafruit board | Arcada + TFLite |
| `StandardFirmataBLE` | 📚 external SW | Firmata + host program. Not a core issue |
| `homekit_lightbulb` | 📚 external library | `BLEHomekit` — outside the Adafruit repository |

**Legend**
- 🚫 **permanently impossible** — the chip/stack structure rules it out. Not planned
- 🔧 **hardware present, no driver** — we can build it
- 📦 **external part needed** — the core does not block it; it needs the part to be tested
- 🧩 **for another board** — meaningless without that board. Use a replacement example if there is one
- 📚 **external software** — not a core issue

## Removal rules — remove only after checking one by one

To remove an example from the count, **both** must hold.

1. It is tied to an external device or board-specific hardware, so whether it builds says nothing about
   our BLE support
2. **The BLE APIs that example uses also appear in the remaining examples** — i.e. no API drops out of
   sight when it is removed

Rule 2 is the key. Not checking it in the bulk removal made `configUuid` · `getAddr` ·
`configAttrTableSize` disappear entirely. When removing, extract the example's BLE classes and calls
and compare against the remaining examples:

```sh
grep -ohE '\b(BLE[A-Z][A-Za-z]*|Bluefruit\.[A-Za-z]+)' <example>.ino | sort -u
```

⚠ The next three are certainly beyond us. But they **stay in the denominator** —
removing them would make us re-investigate "why is it missing?".
- `neopixel` `neomatrix` `gpstest_swuart` — bit-banging. The SoftDevice owns the top interrupt priority,
  so **the behaviour** is structurally broken (CLAUDE.md §7 F6).
  But **they do compile** — `neopixel` actually passes

⚠ **`nfc_to_gpio` is not "structurally impossible", just not implemented.**
It was once written that "the nRF54L has no NFC hardware" — **that was wrong.** **Both** L15 and L05
have NFCT — `NRF_NFCT_NS_BASE = 0x400D6000`, `NFCT_IRQn = 214`, pins
NFC1/NFC2 = P1.02/P1.03 (`docs/PERIPHERAL-PINMAP.md`). nrfx has an NFCT driver too.
The only thing in the way is **that we have not built an NFC driver**.

> ⚠ This error happened by asserting without checking, **even though the counter-evidence was already in
> our repository** (`nrf54l_domains.h` lists NFCT in the P1 domain).
> "The chip does not have it" is a strong claim. Open the MDK headers before making it.

⚠ USB is different. **The nRF54L15 really has no USB hardware** — that is correct
(the nRF54LM20A has it, and M6 looks at it).

⚠ Examples that **verify BLE itself** stay, of course, even if they need external devices:
`central_*` · `dual_bleuart` · `rssi_proximity_*` (two boards), `pairing_*` ·
`blehid_*` · `ancs` · `client_cts` (a phone is enough), `throughput` (throughput measurement).

⚠ `adc` · `hwpwm` · `Fading` and so on in `Hardware/` are unrelated to BLE but
**are within our M2 (Arduino API) goal**. They are counted separately in the table below.

## Results after installing external libraries

Installed: `Adafruit NeoPixel` · `Adafruit GFX` · `Adafruit SSD1306` · `Adafruit BusIO` ·
`MIDI Library` · `Servo`.

| Example | Result | Real cause |
|---|---|---|
| `neopixel` | ✅ compiles | **Our core lacked `interrupts()`/`noInterrupts()`.** Added. (It still does not work because of bit-banging) |
| `neomatrix` | ❌ no `Wire.h` | Our M2 (`Wire` not implemented) |
| `ancs_oled` | ❌ no `Wire.h` | Our M2 |
| `client_cts_oled` | ❌ no `Wire.h` | Our M2 |
| `blemidi` | ✅ compiles and runs on hardware | **It was our BLE API gap.** Solved by adding `BLEMidi` (2026-09-12) |

**Two were reclassified.** `blemidi` was not "external library not installed" but our BLE API gap,
and `neopixel` was our core API gap. Without measuring, both would have stayed blamed on someone else.

## Status

| | Count |
|---|---|
| **Compiles** | **32** |
| Our BLE API missing | about 5 |
| Our M2 (Arduino API) missing | about 20 |
| External library / external device | about 14 |
| Total | 71 |

⚠ The three rows below are **estimates not yet re-measured exhaustively**. Only the pass count is measured.
Re-measuring exhaustively every time a feature is added is the accurate way (last section).

**⚠ Compiling does not mean working.** An asterisk (*) marks those also confirmed on hardware.

### Passing (32)

```
bleuart*  bleuart_multi*  beacon*  eddystone_url*
central_scan*  central_bleuart*
blinky*  rtos_scheduler*  SerialEcho*  temp_measure_blocking*
adv_advanced*  rssi_callback*
blinky_ota  temp_measure_non_blocking  adv_AdafruitColor  rssi_poll
pairing_pin  pairing_passkey  clearbonds  central_pairing
blehid_keyboard  blehid_mouse  blehid_gamepad*  blehid_camerashutter  blehid_keyscan
throughput  central_throughput  blemidi*  client_cts*  ancs*  central_hid*
neopixel(compiles only — bit-banging, cannot work)
```

**16 confirmed on hardware** (asterisk). Our `blehid_button` example was confirmed on hardware too —
pair from the host and press the button, and the key is typed (counted separately, since it is not upstream).

How the hardware checks were done:
- `blinky` · `rtos_scheduler` — no serial output. **The LED GPIO was read over SWD** to confirm toggling
  (P2.00, `OUT` = `0x50050400`).
  ⚠ On nRF54L the GPIO `OUT` is at **offset 0x000**. Reading nRF52's 0x504 always gives 0
- `SerialEcho` — the sent string comes back as is. It also confirms the serial receive fix (§2.5)
- `temp_measure_blocking` — 38.25 °C. ⚠ **9600 baud** (not 115200)
- `adv_advanced` — "Advertising is started"
- `rssi_callback` — `Rssi = -40` after connecting
- `bleuart` — `docs/HIL/M3-softdevice.md` §3.9
- `bleuart_multi` — phone + Mac, two links at once, data both ways (`docs/STATUS.md` B5)
- `central_scan` — receives nearby advertising, parses address·RSSI·AD, 16-bit UUID filter confirmed on hardware
- `central_bleuart` — scan · connect · GATT discovery · notifications · data both ways. Confirmed between boards
  and against `extras/mac_peripheral.py`. The DIS/battery clients were only compiled
#### Not run yet, and why

| Example | Status |
|---|---|
| `temp_measure_non_blocking` | ❌ **The nrfx TEMP IRQ is not wired in our core.** The callback never comes. The blocking version works |
| `adv_AdafruitColor` | ⚠ Advertising was not picked up. Cause unknown (that example has `Serial.begin` commented out, so no clues) |
| `rssi_poll` | Not run yet |
| `blinky_ota` | Not run yet |

⚠ The four HID examples were also **only compiled.** The reason is particular —
**Apple does not expose the HID service (0x1812) to apps.** It is a profile the system handles itself and
CoreBluetooth filters it out, so bleak cannot see even the GATT layer.
The only way to check is to pair directly in the host's Bluetooth settings and press keys
(which is why `examples/Peripheral/blehid_button` was made — one key per button).

⚠ The four pairing examples (`pairing_pin` · `pairing_passkey` · `clearbonds` ·
`central_pairing`) were also **only compiled.** Pairing and bonding themselves were demonstrated against a Mac
with a separate test sketch (`docs/STATUS.md` B10·B11), but that is not flashing and running these examples.
The asterisk goes only on examples that were actually run.
- `beacon` / `eddystone_url` — the advertising payload was scanned with bleak and checked byte by byte.
  Confirmed down to iBeacon major/minor going out **big-endian** and EddyStone URL compression codes
  following the spec

### What would unlock how many

| Feature | Examples blocked | Notes |
|---|---|---|
| Client / Central stack | ~7 | **Done.** `BLEAncs` · `BLEClientCts` · `BLEClientHidAdafruit` are all in (2026-09-12) |
| M2 — SPI / Wire / PDM / PWM | ~16 | Arduino API. Unrelated to BLE |
| HID — gamepad / client | 2 | `BLEHidGamepad` (+`hid_gamepad_report_t`), `BLEClientHidAdafruit` |



### External libraries not installed yet

`Adafruit_Arcada` (large dependency tree) · `SdFat` · `PDM` ·
`Adafruit_CircuitPlayground` · `APDS9960` · `Firmata` · `SoftwareSerial` ·
`BLEHomekit` (not in the registry) · TFLite.

⚠ These too **have to be installed and measured** to tell whether the cause is on our side. `blemidi` and
`neopixel` were overturned exactly that way above.

---

## How to re-measure

```sh
# 1. Fetch the examples and make copies with the three include lines removed
# 2. Compile each; if the storage-space line appears, it passed
for d in */; do
  arduino-cli compile --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 "$d" 2>&1 \
    | grep -q "저장 공간" && echo "PASS $d" || echo "FAIL $d"
done
```

(`저장 공간` is the Korean-locale storage-space line of arduino-cli; match `Sketch uses` on an English locale.)

Looking only at the first error hides the dependencies behind it. **Re-measuring every time a feature is
added** is the accurate way — in practice a small API bundle took it from 7 → 12.

⚠ **The denominator is 71.** Do not trim it in advance. To remove one, check both conditions of §Removal rules
and record the reason in this document at that time.
