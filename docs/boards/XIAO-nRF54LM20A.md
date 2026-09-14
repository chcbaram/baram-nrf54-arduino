# XIAO nRF54LM20A / Sense (Seeed)

*[English](XIAO-nRF54LM20A.md) · [한국어](XIAO-nRF54LM20A.ko.md)*

**Chip: nRF54LM20A, package FCCSP98 (3.67×3.85mm).**
Based on schematic `XIAO_nRF54LM20A_Schematic.pdf` V1.0 (2026-04-09, KiCad).

| | |
|---|---|
| variant | `xiao_nrf54lm20a` — FQBN `baram-nrf54:nrf54l:xiao_nrf54lm20a` |
| Board library | `libraries/BOARD-XIAO-nRF54LM20A` — nPM1300 `PMIC`, Sense IMU `IMU`, examples |
| Memory layout | The nRF54LM20A section of [MEMORY-MAP.md](../MEMORY-MAP.md) |
| Hardware log | `docs/HIL/M6-xiao-nrf54lm20a.md` (2026-09-14, Korean) |

> ⚠ **There is no chip antenna.** RF goes out only through the u.FL connector. Without an external
> antenna, BLE **runs normally and nobody hears it** (§3 RF).
>
> ⚠ **The debug port ships locked (APPROTECT).** The first time, flash with
> `Erase all + Burn SoftDevice (probe-rs)`. The Seeed factory firmware is erased.
>
> ⚠ **The Sense sensors are powered by nPM1300 LDO1, not by a GPIO** (§3 PMIC).

The Sense and plain models **share the same PCB** and differ only in whether the IMU and microphone
are fitted (the two parts are drawn in a dashed box on the block diagram). They share one board entry.
On the plain model `IMU.begin()` returns 0.

---

## 0. Sources and cross-checks

| Source | What it confirmed |
|---|---|
| Schematic PDF | Net names (`P1.00/A0/D0` form), onboard parts, power rails, header physical layout |
| Zephyr `boards/seeed/xiao_nrf54lm20a/` | D0–D27 gpio-map, LED/button, bus assignment (uart20/21, i2c22/30, spi23), crystal caps |
| sdk-nrf-bm v2.0.1 `boards/nordic/bm_nrf54lm20dk/` | Memory partitions, crystal caps (same values as Zephyr) |
| Pin Planner `mcus/nrf54lm20a/fccsp98-3.67x3.85-paaa.json` | AIN assignment, peripheral constraints |

The schematic, Zephyr and Pin Planner **agree on the D numbers, LEDs, button and AIN assignment.**

**How the package was settled**: counting GPIO on the schematic gives P0=10 / P1=32 / P2=11 / P3=13,
66 in total. Of Pin Planner's three packages **only FCCSP98** matches that exactly
(CSP61 has 40, QFN52 has 32, and **QFN52 has no P3 at all**).
The FICR `INFO.PACKAGE` on the board also reads `"PA"`.

---

## 1. How it differs from the XIAO nRF54L15 (read first)

| | XIAO nRF54L15 | **XIAO nRF54LM20A** |
|---|---|---|
| GPIO ports | P0 P1 P2 | P0 P1 P2 **P3** |
| RRAM / RAM | 1.5 MB / 256 KB | **2 MB / 512 KB** (RAM ends at `0x2007FD40`) |
| Antenna | Onboard + u.FL, RF switch | **u.FL only** |
| `Serial` pins | P1.08 / P1.09 | **P1.11 / P1.10** (P1.08/09 are header D6/D7) |
| User LED | 1, **P2.00 (no PWM)** | **RGB, all three on P1 → PWM and interrupts work** |
| Header pins | D0–D10 (14 pins) | **D0–D27** (D11–D18 are pads on the back) |
| SERIAL instances | ~22 | **~24** (SPIM/TWIM/UARTE 23·24 added) |
| PMIC | None | **nPM1300** (charger and regulators) |
| Sense sensor power | GPIO load switch | **PMIC LDO1** |
| Onboard flash | None | **PY25Q64** (8 MB, P2.00–P2.05) |
| As shipped | Can be flashed | **APPROTECT locked** |

**The most important chip difference is P3.** P3 **shares domain 20 peripherals with P1** —
SPIM/TWIM/UARTE 20–24, PWM20–22 and GPIOTE20 reach P3 too.
The L15 rule "one port per domain" widens on LM20A to **"domain 20 owns P1 and P3"**
([PERIPHERAL-PINMAP.md](../PERIPHERAL-PINMAP.md) §5). The core handles it with `NRF54L_IS_DOMAIN20_PORT()`.

There are also things **P3 does not have** — SAADC, PDM, TDM and QDEC are P1-only.

---

## 2. Expansion header

### Physical layout — connector U7 (XIAO-Add-On-Plus)

```
 1  D0   P1.00        14  VBUS
 2  D1   P1.31        13  GND
 3  D2   P1.30        12  3V3_OUT
 4  D3   P1.29        11  D10  P1.06
 5  D4   P1.03        10  D9   P1.05
 6  D5   P1.07         9  D8   P1.04
 7  D6   P1.08         8  D7   P1.09

15  D19  P0.00        23  D27  P3.11
16  D20  P0.01        22  D26  P3.10
17  D21  P0.02        21  D25  P3.09
18  D22  P0.03
19  D23  P0.04
20  D24  P0.05
```

1–14 are the standard XIAO layout, and 15–23 are the Add-On-Plus extension.
⚠ **D11–D18 (P3.00–P3.07) are not on the header but on test points TP11–TP18 on the back.**
3V3_OUT is not the chip supply (VSYS_3V3) but the output of a separate DC/DC (TPS62843).

### XIAO header (D0–D27)

| D | Name | GPIO | | D | Name | GPIO |
|---|---|---|---|---|---|---|
| 0 | D0 / A0 | P1.00 | | 14 | D14 | P3.03 |
| 1 | D1 / A1 | P1.31 | | 15 | D15 | P3.04 |
| 2 | D2 / A2 | P1.30 | | 16 | D16 | P3.05 |
| 3 | D3 / A3 | P1.29 | | 17 | D17 | P3.06 |
| 4 | D4 / SDA / A7 | P1.03 | | 18 | D18 | P3.07 |
| 5 | D5 / SCL / A8 | P1.07 | | 19 | D19 | P0.00 |
| 6 | D6 / TX | P1.08 | | 20 | D20 | P0.01 |
| 7 | D7 / RX | P1.09 | | 21 | D21 | P0.02 |
| 8 | D8 / SCK / A6 | P1.04 | | 22 | D22 | P0.03 |
| 9 | D9 / MISO / A5 | P1.05 | | 23 | D23 | P0.04 |
| 10 | D10 / MOSI / A4 | P1.06 | | 24 | D24 | P0.05 |
| 11 | D11 | P3.00 | | 25 | D25 | P3.09 |
| 12 | D12 | P3.01 | | 26 | D26 | P3.10 |
| 13 | D13 | P3.02 | | 27 | D27 | P3.11 |

The table is keyed by D number, not by physical pin — the physical layout is above.

⚠ **`A8` is not an analog input.** The net is named `P1.07/SCL/A8/D5`, so it looks like A8,
but what P1.07 carries is `SAADC.EXTREF` (external reference), not an AIN.
The schematic MCU symbol also labels this pin `P1.07/EXTREF`. The variant does not define `PIN_A8`.

**There are eight real AINs**: A0=P1.00 (AIN0), A1=P1.31 (AIN1), A2=P1.30 (AIN2),
A3=P1.29 (AIN3), A4=P1.06 (AIN4), A5=P1.05 (AIN5), A6=P1.04 (AIN6), A7=P1.03 (AIN7).
A4–A7 share pins with SPI/I2C, so they cannot be used at the same time.

---

## 3. Onboard parts

### Bus assignment (variant)

| Arduino | Instance | Pins | Notes |
|---|---|---|---|
| `Serial` | UARTE20 | TX P1.11 / RX P1.10 | Onboard SAMD11 → USB CDC |
| `Serial1` | UARTE21 | TX P1.08 (D6) / RX P1.09 (D7) | Header |
| `Wire` | TWIM22 | SDA P1.03 (D4) / SCL P1.07 (D5) | Header |
| `Wire1` | TWIM30 | SDA P0.08 / SCL P0.07 | Sense IMU |
| `SPI` | **SPIM23** | SCK P1.04 / MISO P1.05 / MOSI P1.06 | Header. Domain 20, so 8 Mbps max |
| `SPI1` | SPIM00 | SCK P2.01 / MOSI P2.02 / MISO P2.04 | Onboard flash |
| PMIC bus | **TWIM24** | SCL P1.17 / SDA P1.18 | Used by the board library |

The numbers are all different, so no hardware block is used twice ([PERIPHERAL-PINMAP.md](../PERIPHERAL-PINMAP.md) §0).
It is the same assignment as the Zephyr board definition, except the PMIC bus (Zephyr bit-bangs it).

### RF — an external antenna is required

The chip's ANT output goes through a matching network (L5·L6·L7, C38·C39) and 0 Ω (R25) to
**ANT2 (u.FL connector)** only. There is no chip antenna and no RF switch. The block diagram also
shows only "IPEX" on the RF side.

**Measured (2026-09-14)**:

| | Result |
|---|---|
| No antenna | SoftDevice advertising `start()`·`isRunning()` normal, `RADIO.STATE` reads TX over SWD, `FREQUENCY` alternates 2402/2480 MHz. **But a Mac right next to it received this board 0 times out of 746 advertising reports in 20 s** |
| Antenna attached | Found in 0.4 s, **RSSI −31 dBm**, connect · MTU 247 · NUS echo all work |

Everything looks normal on the software side, so **it is easy to misdiagnose as a core problem.**
If BLE is not heard, check the antenna first.

### LED — one RGB, **active LOW**, all on P1

The common anode of the RGB LED (LED1) is VSYS_3V3, and each cathode goes to its pin through 2 kΩ (R30–R32).

| | GPIO | variant |
|---|---|---|
| Red | P1.22 | `PIN_LED1` = `LED_BUILTIN` = `LED_RED` |
| Blue | P1.23 | `PIN_LED2` = `LED_BLUE` = `LED_CONN` |
| Green | P1.24 | `PIN_LED3` = `LED_GREEN` |

They are on P1, so **all three can do `analogWrite` and `attachInterrupt`** (confirmed on hardware, `rgb_led` example).
`LED_BUILTIN` as red and the connection indicator as blue follow the Adafruit board convention.
The charge LED (D2, red) is driven directly by the nPM1300.

### Button

| | GPIO | |
|---|---|---|
| K2 `USR_KEY` | **P0.09** | External 100 kΩ pull-up (R27), TVS (D4). LOW when pressed |
| K1 | nRF54_RESET | Cannot be read by the MCU |

On P0, so interrupts go through GPIOTE30 (confirmed on hardware, `button` example).

### PMIC — nPM1300

| Rail | Output | Setting | Use |
|---|---|---|---|
| VSYS_3V3 | **BUCK2** | VSET2 = 470 kΩ → 3.3 V | **Chip and LED supply** |
| — | BUCK1 | VSET1 = GND → off | Unused ("Do not use VOUT1") |
| IMU&MIC_3V3 | **LDO1** | Set by register | Sense IMU and microphone |

| Signal | GPIO |
|---|---|
| PMIC SCL | P1.17 |
| PMIC SDA | P1.18 |
| npm_GPIO0 | P1.25 |
| npm_GPIO1 | P1.26 |

- I2C address **0x6B**. External 4.7 kΩ pull-ups (R12/R13) to VSYS_3V3
- Zephyr drives this bus as **`gpio-i2c` (bit-banging)**, but our core does not support bit-banging
  (CLAUDE.md §7 F6). On P1 it works with hardware TWIM, so it uses **TWIM24**, which does not
  collide with the other buses. Responds on hardware
- **There is no battery divider.** Battery voltage, charge state and USB power presence are measured by the PMIC
  (`PMIC.batteryMillivolts()`, `chargeStatus()`, `vbusPresent()`)
- ⚠ Writing the wrong regulator register can switch off **the chip's own supply (BUCK2)**.
  The board library only touches LDO1 and the ADC and does not expose register writes

### Sensors (Sense model)

| | Part | GPIO |
|---|---|---|
| IMU | **LSM6DS3TR-C** @ 0x6A | SDA P0.08 / SCL P0.07 (Wire1), INT1 P0.06, CS P3.12 |
| Microphone | MSM261DGT006 (PDM) | CLK P1.13 / DATA P1.14 (PDM20) |

⚠ **Both sensors are powered by nPM1300 LDO1.** Not only the IMU but also the 4.7 kΩ pull-ups on its bus
(R28/R29) and the CS pull-up (R37, 100 kΩ) hang off that rail. So while LDO1 is off,
**the Wire1 bus itself floats and no address answers.**

Measured (`imu` example): LDO1 off → `WHO_AM_I` no answer, LDO1 on at 3.3 V → `0x6A`.
`IMU.begin()` switches the rail on through the PMIC.

- **LDO1 is set to 3.3 V.** The Zephyr board definition switches it on at 1.8 V, but the schematic rail is named
  `IMU&MIC_3V3`, the block diagram says "LDO 3.3V", and the bus pull-ups are on this rail, so an nRF running at
  3.3 V may not read a 1.8 V HIGH as logic 1. Normal operation at 3.3 V was confirmed
- CS (P3.12) is held in I2C mode by its pull-up. ⚠ **Do not drive P3.12 as an output.**
  Driving it HIGH while the rail is off back-powers the IMU through its protection diode
- Microphone: the core has no PDM API yet

### Onboard flash — PY25Q64 (8 MB)

Nets read directly from schematic p.6 (they match the binding previously derived from the Pin Planner `sQSPI` constraints):

| Signal | GPIO |
|---|---|
| IO3 / HOLD | P2.00 |
| CLK | P2.01 |
| IO0 / MOSI | P2.02 |
| IO2 / WP | P2.03 |
| IO1 / MISO | P2.04 |
| CS | P2.05 |

100 kΩ pull-ups (R42–R44) to VSYS_3V3. The variant puts it on `SPI1` (SPIM00) as standard SPI and
defines `PIN_FLASH_CS/WP/HOLD`. Measured JEDEC ID **`85 20 17`** (Puya, 2^23 B),
SFDP signature and rev 1.0 correct (`flash_id` example).

### Debug / UART

The onboard **ATSAMD11D14A** acts as CMSIS-DAP and USB CDC (the same structure as the XIAO nRF54L15).
It shows up in `probe-rs list` as `Seeed Studio XIAO nRF54LM20A CMSIS-DAP` (`2886:0068`).
It connects to the nRF through a UM3204H level buffer.

| | GPIO |
|---|---|
| nRF54_TX (→ SAMD11) | P1.11 |
| nRF54_RX (← SAMD11) | P1.10 |

SWD is also available on the back test points TP1–TP8.

⚠ **The nRF54LM20A's USB (D+/D-, VBUS) is not connected on this board** (schematic K4·K5 unconnected,
R23 not fitted). USB-C goes only to the SAMD11.

### Clock

| | Part | External caps | variant |
|---|---|---|---|
| LFXO | X1 32.768 kHz, 7 pF ±20 ppm, P1.20/P1.21 | **None** | `LFXO_LOAD_CAP_FF 17000` |
| HFXO | X2 32 MHz, 8 pF ±10 ppm | **None** | `HFXO_LOAD_CAP_FF 15000` |

The schematic notes "The internal capacitance of the matching capacitor is configurable".
The values come from the Zephyr XIAO board definition, and the sdk-nrf-bm LM20 DK uses the same.

Measured: the INTCAP computed for this unit (LFXO 23 / HFXO 40) was written to the registers as is.
**The LFXO is −49 ppm against the host** (5 minutes, 151 samples). That is within the ±250 ppm declared to
the SoftDevice, so BLE is fine, but beyond the crystal spec (±20 ppm). Negative = slow = too much load
capacitance, so reducing 17000 fF may bring it in (not tried).

### NFC

| | GPIO |
|---|---|
| NFC1 / NFC2 | P1.01 / P1.02 |

Goes to the N1/N2 pads on the back through 0 Ω (R39/R40). No antenna is fitted.

---

## 4. Still to do

- [ ] LFXO load cap tuning (the −49 ppm above)
- [ ] PDM microphone — the core has no PDM API
- [ ] Low-power measurement (CLAUDE.md §7 F8 — probe disconnected)
- USB — the chip has USBHS, but **this board has no wiring for it**, so it does not apply

### Board library example TODO

Present now: `pmic` `imu` `flash_id` `rgb_led` `button` `i2c_scan` (all confirmed on hardware).

| | Example | Content | Readiness |
|---|---|---|---|
| [x] | `i2c_scan` hardware check | IMU (0x6A) found on Wire1 and `WHO_AM_I` read | Done |
| [ ] | **`ble_imu`** | Send IMU values over BLE (BLEUart or a custom service) | Ready now. There is no example that uses Sense and BLE together |
| [ ] | **`ble_battery`** | PMIC battery voltage → `BLEBas` level (%) | Ready now. Voltage → % from a rough table. Ties in with HOGP requiring BAS (STATUS) |
| [ ] | **`flash_storage`** | Erase a sector → write → read back on the onboard 8 MB flash | Ready now. Today it only reads the ID |
| [ ] | `imu_wakeup` | Wake from standby on motion/tap via INT1 (P0.06), switch the sensor rail on/off | Check the interrupt registers (TAP_CFG, WAKE_UP_THS, MD1_CFG, …) against the ST driver first. Measure current with the probe removed (CLAUDE.md §7 F8) |
| [ ] | Microphone recording / level | PDM microphone (Sense) | **The core needs a PDM API first** |

### IMU API TODO

| | Item | Why |
|---|---|---|
| [ ] | Add `temperatureSampleRate()` | The last method missing to match Arduino_LSM6DS3 exactly |
| [ ] | Decide whether to follow Arduino_LSM6DS3 filter settings | It uses `CTRL1_XL 0x4A` (LPF1) and `CTRL8_XL 0x09`. The noise character of readings changes |
| [ ] | Consider Seeed `LSM6DS3` API compatibility | Needed to port XIAO nRF52840 Sense sketches (`LSM6DS3 myIMU(I2C_MODE, 0x6A)`, `readFloatAccelX()`). On this board the Seeed library uses `Wire` (header) and does not switch LDO1 on, so it does not work as is. ⚠ A compatible class would clash on `LSM6DS3.h` · `LSM6DS3` for users who also install the Seeed library |
