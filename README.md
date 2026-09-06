# BARAM nRF54L Arduino Core

**An Arduino core for the Nordic nRF54L series — built so that existing Adafruit
Bluefruit (nRF52) sketches keep working.**

*[English](README.md) · [한국어](README.ko.md)*

[![License: MIT](https://img.shields.io/badge/core-MIT-blue.svg)](LICENSE)
[![SoftDevice](https://img.shields.io/badge/SoftDevice-S145%20v10.0.1-orange.svg)](docs/LICENSE-INVENTORY.md)
[![Status](https://img.shields.io/badge/status-M1%20(pre--release)-yellow.svg)](docs/STATUS.md)

> ### ⚠ Pre-release
> Blink, `Serial`, multitasking and tickless idle run on real hardware across three
> boards. **BLE is not implemented yet** (milestone M3), and there is **no Board
> Manager release yet** — see [Installation](#installation).
> Current state: [docs/STATUS.md](docs/STATUS.md)

---

## Contents

- [Why this exists](#why-this-exists)
- [Features](#features)
- [Supported boards](#supported-boards)
- [Installation](#installation)
- [Your first sketch](#your-first-sketch)
- [Support scope](#support-scope)
- [How it is built](#how-it-is-built)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license--mixed-not-open-source)

---

## Why this exists

The nRF54L is Nordic's successor to the nRF52, and the nRF52 carries one of the
largest bodies of Arduino BLE code in existence — Adafruit's Bluefruit ecosystem.
None of it carries over on its own.

Every other nRF54L Arduino effort exposes a **third, project-specific BLE API**.
That is a defensible choice, but it means a working nRF52 sketch gets rewritten from
scratch, and the libraries and examples built around `Bluefruit.begin()` are worth
nothing on the new chip.

This core takes the opposite position:

> **When portability and Adafruit compatibility conflict, compatibility wins.**

The goal is that an `.ino` written for a Bluefruit Feather compiles and runs on an
nRF54L board with minimal edits — same `Bluefruit` API, same `Scheduler.startLoop()`,
same `delay()` semantics on top of FreeRTOS.

A second goal shaped just as much of the design: **custom boards.** Getting firmware
onto a board you built yourself, and updating it in the field over UART or BLE,
should not require a debug probe attached to every unit.

## Features

| | |
|---|---|
| **Your nRF52 sketches keep working** | Bluefruit-compatible API, `SchedulerRTOS`, AVR compatibility shims. Migration is the reason this project exists, not a side effect |
| **A qualified BLE stack** | Nordic SoftDevice **S145** — peripheral *and* central, Bluetooth-qualified. Not a reimplementation |
| **Nothing else to install** | Board Manager brings the core, the compiler and the flashing tool. No Python, no SDK, no `west` |
| **Small and offline-friendly** | The platform archive is **1.7 MB**. An SDK-based toolchain is a multi-gigabyte first install |
| **Cross-platform** | macOS, Linux and Windows, on x86-64 and arm64 |
| **Real FreeRTOS, tickless** | GRTC-based tick, measured at **0 ppm** against the hardware counter, with `millis()` staying accurate through sleep |
| **Bootloader on the roadmap** | UART and BLE OTA DFU (M4), with the SWD path kept working alongside it |

**What this is not.** It is not faster or more capable at BLE than a Zephyr/NCS-based
core. Those use Nordic's SoftDevice Controller, the same qualified controller family
as the SoftDevice used here. The difference is the API you write against, the size of
the install, and where the project is going — not radio quality.

## Supported boards

| Board | MCU | Flash / RAM | Debug | Pinout |
|---|---|---|---|---|
| **Seeed XIAO nRF54L15** / Sense | nRF54L15 | 1.5 MB / 256 KB | **onboard CMSIS-DAP** | [docs](docs/boards/XIAO-nRF54L15.md) |
| **NU54-DK** | nRF54L05 | 500 KB / 96 KB | external probe | [docs](docs/boards/NU54-DK.md) |
| **NU54V-DK** | nRF54L15 | 1.5 MB / 256 KB | external probe | [docs](docs/boards/NU54-DK.md) |

All are 128 MHz Cortex-M33. **The XIAO needs nothing but a USB-C cable** — its onboard
debugger handles both flashing and the serial console, so it is the easiest board to
start with.

Memory layout is per chip rather than per board: [docs/MEMORY-MAP.md](docs/MEMORY-MAP.md).

> Adding a board means one `variants/` directory and one entry in `boards.txt`.
> [docs/boards/XIAO-nRF54L15.md](docs/boards/XIAO-nRF54L15.md) is a worked example.

## Installation

### Board Manager — *not published yet*

This is how it will work once the first release is tagged. **The URL below does not
resolve yet**; watch [docs/STATUS.md](docs/STATUS.md) for the release.

1. Open **Arduino IDE → Preferences**
2. Paste this into **Additional boards manager URLs**:
   ```
   https://raw.githubusercontent.com/chcbaram/baram-nrf54-arduino/main/package_baram_nrf54_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search for `nRF54L`, install
   **BARAM nRF54L Boards**
4. Select your board under **Tools → Board → BARAM nRF54L Boards**

Board Manager also installs the Arm toolchain and `probe-rs`, so there is nothing
else to set up.

### From source — works today

Until the release exists, install by placing the repository in your sketchbook's
`hardware/` directory. A symlink keeps your git checkout wherever you like.

```sh
git clone https://github.com/chcbaram/baram-nrf54-arduino
mkdir -p ~/Documents/Arduino/hardware
ln -s "$(pwd)/baram-nrf54-arduino" ~/Documents/Arduino/hardware/baram-nrf54
```

Your sketchbook path is whatever `arduino-cli config get directories.user` reports
(**Preferences → Sketchbook location** in the IDE).

> **The link must be named `baram-nrf54`.** Installed this way, the first part of the
> FQBN comes from the directory name; installed through Board Manager it comes from
> the package index. Naming it anything else gives you an FQBN that will not match a
> released install.

You then supply the two tools yourself, because `platform.txt` expects Board Manager
to have provided them:

| Tool | Version | Notes |
|---|---|---|
| [xPack arm-none-eabi-gcc](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v14.2.1-1.1) | **14.2.1-1.1** | The release pins this version. Another one builds fine but produces different sizes |
| [probe-rs](https://github.com/probe-rs/probe-rs/releases/tag/v0.32.0) | **0.32.0** | A macOS build is bundled in `nrf54l/tools/` |

Point the core at them by copying the example and editing the paths:

```sh
cp nrf54l/platform.local.txt.example nrf54l/platform.local.txt
```

Full walkthrough, including what breaks if you skip this:
[docs/STATUS.md § 다른 PC에서 이어서 작업하기](docs/STATUS.md).

## Your first sketch

```cpp
void loop2()
{
  digitalToggle(LED_CONN);
  delay(500);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_CONN, OUTPUT);

  Scheduler.startLoop(loop2);      // a second task, Adafruit's rtos.h API
}

void loop()
{
  digitalToggle(LED_BUILTIN);
  delay(1000);
  Serial.printf("millis=%lu micros=%lu\n", millis(), micros());
}
```

`loop()` and `loop2()` are separate FreeRTOS tasks, and `delay()` yields rather than
spinning — this is the same behaviour as the Adafruit nRF52 core.

Upload with the IDE's **Upload** button, or:

```sh
arduino-cli compile --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 <sketch>
arduino-cli upload  --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 <sketch>
```

Then open the Serial Monitor at **115200 baud**.

## Support scope

**Supported** — GPIO, `millis()` / `micros()` / `delay()`, `Serial`, `SchedulerRTOS`,
FreeRTOS with tickless idle.

**Coming** — `analogRead` / `analogWrite`, `Wire`, `SPI`, `attachInterrupt` (M2);
BLE (M3); bootloader with UART and BLE OTA DFU (M4).

**Not supported**

- **ArduinoBLE** — it speaks HCI and the SoftDevice does not expose HCI.
  Structurally impossible, not a matter of effort.
- **Bit-banged libraries** (NeoPixel, DHT, OneWire, SoftwareSerial) — the SoftDevice
  owns the top interrupt priority and blocks the application during radio events.
  Use PWM + EasyDMA based alternatives instead.
- **USB** — the nRF54L15 has no USB hardware, so USB CDC, UF2 and the 1200 bps touch
  reset are unavailable on the chips supported today. This is a property of the chip
  rather than a decision. The **nRF54LM20A does have USB** (high-speed USBHS) and is
  planned for M6, at which point USB support will be revisited.
- **Matter / Thread / Zigbee / LE Audio / 802.15.4** — out of scope. This core targets
  BLE applications.

## How it is built

```
Your sketch (.ino)
├─ Bluefruit52Lib-compatible API   ← reimplemented on sd_ble_*
├─ Arduino API                      ← implemented on nrfx
├─ SchedulerRTOS                    ← same API as Adafruit's rtos.h
├─ FreeRTOS (tickless, GRTC tick)
├─ SoftDevice S145 v10.0.1          ← Bluetooth qualified
└─ nrfx
```

The base SDK is [nrfconnect/sdk-nrf-bm](https://github.com/nrfconnect/sdk-nrf-bm)
v2.0.1, the **bare metal** option for the nRF54L series. Zephyr is not used —
the reasoning, and the conditions under which that would be revisited, are in
[CLAUDE.md § 2](CLAUDE.md).

Uploading is **CMSIS-DAP over SWD** driven by `probe-rs`. On the XIAO the probe is
already on the board. UART and BLE OTA DFU arrive in M4; the SWD path stays.

## Troubleshooting

**The board does not appear in the Tools → Board menu.**
Check the directory name is exactly `baram-nrf54` and that it sits directly under
your sketchbook's `hardware/`. `arduino-cli board listall | grep nrf54l` should list
three boards.

**Upload fails with `cannot execute upload tool: fork/exec {runtime.tools....}`.**
`platform.local.txt` is missing or its `probers.path` is wrong. See
[Installation](#from-source--works-today).

**The serial port disappears right after uploading.**
Expected on boards with an onboard debugger: resetting the target re-enumerates the
USB device, so the old port handle goes stale. Reopen the port.

**It builds, but sizes do not match what the documentation reports.**
You are almost certainly on a different compiler version. Set `toolchain.path`
explicitly — otherwise another installed Arduino package's toolchain can be picked up
silently.

**`probe-rs` cannot find a probe.**
`probe-rs list` should show your debugger. On the XIAO it appears as
`Seeed Studio XIAO nrf54 CMSIS-DAP`. On the DK boards, connect an external CMSIS-DAP
probe to the SWD header.

## Contributing

Issues and pull requests are welcome. Two things worth knowing first:

- **[CLAUDE.md](CLAUDE.md) is the design document.** It records the rules, the
  decisions that are settled, and — most usefully — a list of traps that cost real
  debugging time (`§ 7`). Read the relevant entry before touching FreeRTOS, nrfx or
  the build recipes.
- **Hardware claims need hardware evidence.** Measurements go in
  [docs/HIL/](docs/HIL/) with enough detail to reproduce them. Several entries in the
  trap list exist because a datasheet or a comment turned out to be wrong.

## License — **mixed, not "open source"**

The bundled SoftDevice does not meet the OSI definition, so the project as a whole
cannot honestly be called open source.

| Component | License |
|---|---|
| Core code | **MIT** ([LICENSE](LICENSE)) |
| Bundled SoftDevice (S145 hex) | **LicenseRef-Nordic-5-Clause** — **Nordic ICs only**; no modification or reverse engineering |
| Arduino API files (`Print`, `Stream`, `WString`, …) | LGPL-2.1, as in other Arduino cores |
| nrfx / MDK / CMSIS / FreeRTOS | BSD-3-Clause / Apache-2.0 / MIT, per component |

Full inventory: [docs/LICENSE-INVENTORY.md](docs/LICENSE-INVENTORY.md).

This project is not affiliated with or endorsed by Nordic Semiconductor.

## Documentation

| | |
|---|---|
| [CLAUDE.md](CLAUDE.md) | Project rules, known traps, milestones |
| [docs/STATUS.md](docs/STATUS.md) | Where things stand and what is next |
| [docs/boards/](docs/boards/) | One document per board, from the schematics |
| [docs/MEMORY-MAP.md](docs/MEMORY-MAP.md) | RRAM / RAM layout per chip |
| [docs/PERIPHERAL-PINMAP.md](docs/PERIPHERAL-PINMAP.md) | Which pins a given peripheral can reach |
| [docs/HIL/](docs/HIL/) | Hardware verification logs |
| [docs/LICENSE-INVENTORY.md](docs/LICENSE-INVENTORY.md) | Licensing, component by component |

## Credits

- [Adafruit nRF52 Arduino core](https://github.com/adafruit/Adafruit_nRF52_Arduino) — the API this core aims to stay compatible with, and its structural reference
- [nrfconnect/sdk-nrf-bm](https://github.com/nrfconnect/sdk-nrf-bm) — the bare metal SDK and SoftDevice
- [probe-rs](https://github.com/probe-rs/probe-rs) — flashing and debugging
- [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS-Kernel), [nrfx](https://github.com/NordicSemiconductor/nrfx)
