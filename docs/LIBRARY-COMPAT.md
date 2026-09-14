# Third-party library compatibility

*[English](LIBRARY-COMPAT.md) · [한국어](LIBRARY-COMPAT.ko.md)*

The principle of CLAUDE.md §11 — **do not guess, compile it.**
This document is the result of actually building, and it backs the "support scope" in the README.

Measured: 2026-09-12 · XIAO nRF54L15 (`baram-nrf54:nrf54l:xiao_nrf54l15`)

---

## 1. Results

| Library | Bus | Compiles | On hardware |
|---|---|---|---|
| `Adafruit_BME280` (+ `Adafruit_BusIO`, `Adafruit_Unified_Sensor`) | I2C | ✅ | — |
| `Adafruit_seesaw` | I2C | ✅ | — |
| `Seeed_Arduino_LSM6DS3` | I2C | ✅ | ⚠ §3 below |
| `Adafruit_ST7735/ST7789` (+ `Adafruit_GFX`) | SPI | ✅ | — |
| `SdFat` | SPI | ✅ | ✅ NU54-DK + SD socket |
| `SD` (Arduino) | SPI | ✅ | ✅ NU54-DK + SD socket |
| `MIDI_Library` | UART | ✅ | — |
| `ArduinoJson` | — | ✅ | — |
| **`Servo`** | — | ❌ | **Structurally impossible.** §4 |

**"Compiles ✅ / On hardware —"** means there was no part to test with, not that it failed.
Read them separately.

---

## 2. What it took to get here — three sets of shims

At first **even a sketch using only I2C** did not compile. Every cause was a gap in our core,
not a problem in the libraries.

| What was missing | What broke | Added in |
|---|---|---|
| `digitalPinToPort` / `digitalPinToBitMask` / `portOutputRegister` / `portInputRegister` | `Adafruit_BusIO` — **used by almost every Adafruit sensor** | `cores/nrf54l/Arduino.h` |
| `constrain` / `round` / `sq` / `radians` / `bitRead` … | `Adafruit_seesaw` | Same |
| `pins_arduino.h` | `Adafruit_ST77xx` | `cores/nrf54l/pins_arduino.h` |

**⚠ `Adafruit_BusIO` is the gate.** Even for an I2C-only sketch, **the SPI sources of BusIO are compiled
too**, so without `SPI.h` and the AVR port macros it always breaks.
That is why `SPI` had to come first, along with the port shims.

⚠ Adafruit's two-port version (`abs < 32 ? NRF_P0 : NRF_P1`) cannot be used as is.
The nRF54L has **three** ports, the LM20A **four**.

Bonus: `digitalPinHasPWM()` is not left rough like Adafruit's (`P > 1`) but answers correctly.
PWM20/21/22 are domain 20 and attach **only to P1** — it uses the generated `nrf54l_pinmap.h`.

---

## 3. `Seeed_Arduino_LSM6DS3` — compiles, but the values are 0

**Not our problem.** That library applies the `Wire` → `Wire1` substitution
**only for specific Seeed board macros** (`TARGET_SEEED_XIAO_NRF52840_SENSE` and so on).
On our board it uses the header-side `Wire` (TWIM22, D4/D5), where nothing is attached.
The XIAO's onboard IMU is on `Wire1` (TWIM30, P0.03/P0.04).

→ Use a library that takes the bus as an argument, or attach the sensor to the header-side `Wire`.
APIs that accept `begin(addr, &Wire1)`, like `Adafruit_BME280`, have no problem.

**Our `Wire1` itself works** — the `i2c_scanner` example finds the XIAO onboard IMU
at `0x6A` (`docs/STATUS.md` §2.11).

---

## 4. `Servo` — structurally impossible

The library blocks itself:

```
Servo.h:79:2: error: #error "This library only supports boards with an AVR, SAM, SAMD, NRF52 or STM32F4 processor."
```

It is an `#error` that checks architecture macros, so no shim can get past it.
Defining `ARDUINO_ARCH_NRF52` would get through, but **that is a separate decision and
risky** — it opens nRF52 register access paths along with it (CLAUDE.md §11).
For now it stays **unsupported**.

If you need a PWM servo, drive it directly with `analogWrite()`. Servos usually want
50 Hz, so set the frequency with `analogWriteResolution()`
(base clock fixed at 1 MHz → frequency = 1 MHz / 2^bits).

---

## 5. The category that still does not work (CLAUDE.md §7 F6)

**Bit-banging libraries** — NeoPixel, DHT, OneWire, SoftwareSerial and so on.
The SoftDevice owns the top priority and blocks the application during radio events, so
the timing breaks. **This is to be documented, not fixed.**

---

## 6. Reproducing

```sh
arduino-cli compile -b baram-nrf54:nrf54l:xiao_nrf54l15 <sketch>
```

Install the libraries with the Library Manager. An `architectures=` mismatch warning can be ignored —
arduino-cli only warns and carries on compiling (§11).
