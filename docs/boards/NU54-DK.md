# NU54-DK (nRF54L05) / NU54V-DK (nRF54L15)

*[English](NU54-DK.md) · [한국어](NU54-DK.ko.md)*

Analysis of the schematics `NU54_DK.SchDoc` / `NU54_Power.SchDoc` (2025-10-01, edited 2025-10-16).
MCU module: **NCRB54N01VC** (62 pins).

**Both boards share the pin assignment in this document.** Only the fitted module differs;
the schematic and pin map are identical, so the Arduino variant is shared too, as `variants/nu54dk`.
The only difference is memory size, summarised in [MEMORY-MAP.md](../MEMORY-MAP.md).

| | NU54-DK | NU54V-DK |
|---|---|---|
| Chip | nRF54L05 (500KB RRAM / 96KB RAM) | nRF54L15 (1.5MB RRAM / 256KB RAM) |
| FQBN | `baram-nrf54:nrf54l:nu54dk` | `baram-nrf54:nrf54l:nu54vdk` |

> **Key point**: the LED / button / UART pins are **identical to the Nordic nRF54L15 DK**.
> They match sdk-nrf-bm `boards/nordic/bm_nrf54l15dk/include/board-config.h` one to one,
> so Nordic samples run without pin changes.

---

## 1. Pin assignment

### LEDs — **active HIGH**

| Part | Pin | Driver | Gate resistor |
|---|---|---|---|
| D7 | **P2.09** | Q2A (DMN2991 N-MOSFET) | R13 10K pull-down |
| D8 | **P1.10** | Q2B | R14 1M pull-down |
| D9 | **P2.07** | Q3A | R15 1M pull-down |
| D10 | **P1.14** | Q3B | R16 1M pull-down |

The anodes go to 3V3 through 1k (R9–R12) and the cathodes to the MOSFET drains.
Driving the gate HIGH turns the LED on → **`LED_STATE_ON = 1`**.
This is the **opposite** of the Adafruit nRF52 core default (`LED_STATE_ON 0`, active LOW), so the variant has to flip it.

> ⚠ **D9 (P2.07) is shared with pin 6 (SWO) of SWD connector J3.** Turning on SWO tracing makes the LED blink along.

### Buttons — **active LOW, internal pull-up required**

| Part | Pin |
|---|---|
| SW2 | **P1.13** |
| SW3 | **P1.09** |
| SW4 | **P1.08** |
| SW5 | **P0.04** |
| SW1 | RESET (R17 10K pull-up + D6 1N4148) |

The schematic states `USE INTERNAL PULLUP`. There are no external pull-ups → `pinMode(pin, INPUT_PULLUP)` is required.

### UART — CP2102N USB bridge

| nRF54L15 | Direction | CP2102N |
|---|---|---|
| **P0.00** | TX → | RXD (pin 20) |
| **P0.01** | ← RX | TXD (pin 21) |
| **P0.02** | CTS ← | RTS (pin 19) |
| **P0.03** | RTS → | CTS (pin 18) |

> **⚠ Hardware flow control (RTS/CTS) is not used.** This is a project decision.
> With HWFC on, the UARTE waits for the peer to assert CTS and sends not a single byte,
> and host terminals usually do not raise RTS, so `Serial.println()` simply stalls.
> This happened on the hardware.
> **As a result P0.02 / P0.03 are free and were assigned to `Wire` (TWIM30)** — see docs/PERIPHERAL-PINMAP.md.
>
> The UARTE30 PSEL register field order is **TXD, CTS, RXD, RTS** (different from nRF52).

**The instance is `UARTE30`.** Same as the Nordic DK's `BOARD_APP_UARTE_INST` / `BOARD_APP_UARTE_PIN_*`.
P0.x belongs to the always-on low-power domain, and UARTE30/SPIM30/TWIM30 serve that port.

> This is not the Nordic DK's **console** UART (`UARTE20`, P1.04/P1.07). On our board the application
> UART is the one wired to the USB bridge, so `Serial` = UARTE30.

### Clock — LFXO fitted

**Y1 32.768 kHz** (Q13FC13500004) + C13/C14 13pF sit on **P1.00 (XL1) / P1.01 (XL2)**.

- Define **`USE_LFXO`** in the variant
- GRTC `CLKSEL` to LFXO (CLAUDE.md §7 F3)
- **Do not use P1.00 / P1.01 as GPIO.** Leave them out of the pin table or make them no-ops

### Analog / special functions

| Function | Pins |
|---|---|
| AIN0 – AIN3 | P1.04, P1.05, P1.06, P1.07 |
| AIN4 – AIN7 | P1.11, P1.12, P1.13, P1.14 |
| NFC1 / NFC2 | P1.02 / P1.03 |

> AIN6 (P1.13) overlaps SW2, and AIN7 (P1.14) overlaps LED D10.

---

## 2. Debug / programming

**There is no onboard debug probe.** Connect an external CMSIS-DAP probe.

### J3 — ARM Cortex 10-pin 1.27mm (standard layout)

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | VMCU (VTref) | 2 | SWDIO |
| 3 | GND | 4 | SWDCLK |
| 5 | GND | 6 | **SWO = P2.07** |
| 7 | NC (key) | 8 | NC |
| 9 | GND | 10 | nRESET |

### P2 — 5-pin 1.27mm

`1 = VMCU`, `2 = SWDIO`, `3 = GND`, `4 = SWDCLK`, `5 = RESET`

Upload uses **probe-rs** (CLAUDE.md §3).

Probe used for verification: **NU-DAP** — CMSIS-DAP, VID:PID `0d28:0204` (Arm).
Recognised by `probe-rs list`; connect, flash and verify all work with `--chip nRF54L15`.
Writing and verifying a 33 KB hex takes about 3.3 s. `--connect-under-reset` fails on this probe, so do not use it.

---

## 3. Expansion headers

Two 25-pin headers. **Everything is brought out as is**, including P1.00/P1.01 (LFXO).

**P1 header**
```
 1 P0.00   2 P0.01   3 GND    4 P0.02   5 P0.03
 6 P0.04   7 P1.00   8 GND    9 P1.01  10 P1.02
11 P1.03  12 P1.04  13 GND   14 P1.05  15 P1.06
16 P1.07  17 P1.08  18 GND   19 P1.09  20 P1.10
21 P1.11  22 SWDCLK 23 SWDIO 24 GND    25 GND
```

**P3 header**
```
 1 GND     2 GND     3 RESET  4 P1.12   5 P1.13
 6 P1.14   7 P2.10   8 GND    9 P2.09  10 P2.08
11 P2.07  12 P2.06  13 GND   14 P2.05  15 P2.04
16 P2.03  17 P2.02  18 GND   19 P2.01  20 P2.00
21 VMCU   22 3V3    23 GND   24 GND    25 VIN
```

**All of P2.00 – P2.10 is brought out**, so the high-speed domain (SPIM00) is usable. See the SPI notes in CLAUDE.md §4.

---

## 4. Power

```
USB-C (J1) ──┬─ VBUS ─ FB1 ─ CP2102N (U3)
             └─ D2 ─┐
VIN (J2, 5~14V) ─ D1 ─┴─ AZ1117CR-3.3 (U1) ─ 3V3 ─ VMCU
```

- USB-C: 5.1k (R1/R2) on CC1/CC2 — sink only
- Input protection: D11 SD15C, D12 PTVS5V5D1BLYL
- USB and VIN are OR-ed through SBR2U60S1F diodes → both can be applied at once
- **3V3 and VMCU are separate nets.** When measuring current, check which point you are measuring (CLAUDE.md §7 F8)

---

## 5. Summary of points for the Arduino core

| # | Item |
|---|---|
| 1 | LEDs are **active HIGH** → `LED_STATE_ON 1` (opposite of the Adafruit default) |
| 2 | Buttons have no external pull-up → `INPUT_PULLUP` required |
| 3 | **P1.00 / P1.01 are LFXO only** — do not expose them as GPIO |
| 4 | **P2.07 = LED D9 + SWO** — note it in variant.h |
| 5 | `Serial` = **UARTE30** (P0.00/P0.01, flow control P0.02/P0.03) |
| 6 | **Three ports** (P0/P1/P2) — Adafruit's two-port `digitalPinToPort()` has to be extended |
| 7 | No onboard probe — external CMSIS-DAP needed |
| 8 | **`USE_LFXO` must be defined** — without it the clock runs on the internal RC and is 0.9% off (CLAUDE.md §7 F12) |
| 9 | **No flow control** → P0.02 / P0.03 are free. But **do not put `Wire` there** — TWIM30 is the same block as `Serial` (UARTE30). `Wire` is on P1.11/P1.12 (TWIM22) and **overlaps A4/A5** |
| 10 | **Register offsets differ from nRF52** — GPIO `OUT` is at **`0x000`**, not `0x504`. The reset reason is in `NRF_RESET`. Details in [HIL/M1-nu54dk.md](../HIL/M1-nu54dk.md) §3 |
| 11 | A peripheral can only use **GPIO ports owned by its power domain** — [PERIPHERAL-PINMAP.md](../PERIPHERAL-PINMAP.md) |
