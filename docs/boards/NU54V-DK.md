# NU54V-DK (nRF54L15)

*[English](NU54V-DK.md) · [한국어](NU54V-DK.ko.md)*

**Module: NCRB54N01VC. Based on schematic `NU54_DK_2026-07-13T15_16_09_UTC_SCH.pdf`,
`Variant NU-54DK-C` (revised 2026-07-13).**

> ⚠ **This is a different board from the NU54-DK.** For a while the two shared a variant (`nu54dk`),
> but the schematic shows a different PCB. **It was split into `nu54vdk` on 2026-09-12.**

---

## 0. How it differs from the NU54-DK

| | NU54-DK | **NU54V-DK** |
|---|---|---|
| Chip | nRF54L05 | **nRF54L15** |
| Debug | External probe (J3) | **Onboard CMSIS-DAP** |
| USB | CP2102N USB-UART | **USB-C shared with the DAP** |
| Power | AZ1117CR LDO | **BQ25186 battery charger PMIC** |
| Expansion headers | 25 pins × 2 (P1/P3) | **30 pins × 2 (P2/P4)** |
| I2C connector | None | **Qwiic (J5), 2.1K pull-ups** |
| Second UART | None | **Yes** (to the DAP) |

**The same**: four LEDs, four buttons, the `Serial` assignment. See §1.

---

## 1. LEDs · buttons · `Serial` — same as the NU54-DK

| | GPIO |
|---|---|
| LED1 | P2.09 |
| LED2 | P1.10 |
| LED3 | P2.07 (shared with SWO) |
| LED4 | P1.14 (shared with AIN7) |
| SW1 | P1.13 (shared with AIN6) |
| SW2 | P1.09 |
| SW3 | P1.08 |
| SW4 | P0.04 |

The LEDs are **active HIGH** (driven through a buffer + MOSFET). The buttons have no external
pull-ups, so `INPUT_PULLUP` is required — the schematic says `USE INTERNAL PULLUP`.

`Serial` = P0.00 TX / P0.01 RX (+ P0.02 RTS / P0.03 CTS), through the onboard DAP.

> This assignment also matches the Zephyr DTS in `chcbaram/nu54dk` — extracting coordinates
> from the schematic and reading the DTS gave the same values independently.

---

## 2. Qwiic I2C connector (J5) — this is `Wire`

```
J5   1 GND   2 VDD_MOD   3 SDA   4 SCL      (Qwiic standard)
```

| | GPIO | Bridge |
|---|---|---|
| SDA | **P1.02** | SB14 |
| SCL | **P1.03** | SB15 |

**2.1K pull-ups (R29/R30) are on the board.** Do not add external pull-ups.

⚠ P1.02 / P1.03 are the chip's **NFC1 / NFC2 dual-function pins**. On this board they are wired
to Qwiic, so **NFC cannot be used.** That is why the variant does not define `PIN_NFC1/2`.

### ⚠⚠ Right after reset these pins are **NFC pads**, so I2C does not work

The nRF54L **boots with NFC dual-function pins in NFC pad mode.** In that state they work
neither as GPIO nor as TWIM, and **`Wire` finds nothing, with no error and no log.**
It actually stopped us once on this board — the scan came back completely empty.

The MDK startup (`system_nrf54l.c`) does switch it off, but only when
`NRF_CONFIG_NFCT_PINS_AS_GPIOS` is defined, and we do not use that global build flag
(some boards may want NFC).

→ **This board's `initVariant()` switches it off:**

```c
NRF_NFCT->PADCONFIG = (NFCT_PADCONFIG_ENABLE_Disabled << NFCT_PADCONFIG_ENABLE_Pos);
```

If you make a new board that uses the same pins, **copy this line.**

---

## 3. PMIC — BQ25186, on the same bus as `Wire`

Lithium charger **BQ25186** (`VBAT` input, `VDD_3V3_SYS` output).

**Its I2C is the same bus as Qwiic** — it sits on P1.02 / P1.03 at **0x6A**.
A scan on the hardware shows both side by side:

```
I2C scanner
Wire : 0x6A 0x70          <- 0x6A PMIC, 0x70 the SHTC3 on Qwiic
```

⚠ **Plugging a device that uses 0x6A into Qwiic causes a conflict.**

> ⚠ **For a while this document said the PMIC I2C was on P1.11 / P1.12. That was wrong.**
> The port names on the PowerBlock sheet symbol could not be read from the schematic, so it was
> inferred that "TWIM cannot reach P2, so SCL/SDA must be the two P1 pins". The inference was
> logical but **the premise was wrong** — SB1–SB4 do not carry I2C.
> A single scan on the hardware overturned it.

### `SB1`–`SB4` — settled

The PowerBlock port order in the block diagram (`PMIC_INT` → `PMIC_PG` → `PMIC_CE` → `VBAT_MON`)
follows the `SB1`–`SB4` order directly, and **measurement confirms it.**

| Bridge | GPIO | Signal | Measured |
|---|---|---|---|
| SB1 | **P1.11** (= A4) | `PMIC_INT` | digital 1 / 3300 mV — open drain + 10K pull-up, idle when inactive |
| SB2 | **P2.08** | `PMIC_PG` | digital 0 |
| SB3 | **P2.10** | `PMIC_CE` | digital 0 — charge enable. Can be driven as an output |
| SB4 | **P1.12** (= A5) | **`VBAT_MON`** | 2.8 V → battery 4.1 V |

I2C is not on these nets — `SB16` / `SB17` connect `PMIC_SDA` / `PMIC_SCL`
**to P1.02 / P1.03**, the same nets as Qwiic.

### Battery voltage — read directly on `A5`

```
VBAT ─ R8 470K ─┬─ P1.12 (AIN5)
                └─ R11 1M ─ GND
```

Divider ratio 1M/(470K+1M) = **0.680** → `battery voltage = reading × 1.470`.
The variant provides it as `PIN_VBAT` / `VBAT_DIVIDER` (the same names as XIAO).

Cross-check on hardware: ADC 4.05–4.16 V ↔ the charger reports "CV, target 4.20 V". They agree.

⚠ **The divider output impedance is 320 kΩ** (470K ‖ 1M), so with the SAADC default acquisition
time (10 µs) readings come out low and wander by tens of mV. **Average several reads.**
In return the leakage is only **2.8 µA** at 4.1 V, which is the right choice for battery operation.

### Examples — `libraries/BOARD-NU54V-DK/examples/`

| | What | Output on hardware |
|---|---|---|
| `pmic` | Full charger state — state/input/settings/faults/latched | `charging - constant voltage` / `limit 500 mA` / `target 4.20 V  charge 10 mA` |
| `battery` | Voltage and charge state side by side | `4.154 V  ~94%  full or disabled  [VIN good]` |
| `dual_serial` | The two USB ports | Confirmed `[port 1]` / `[port 2]` separate per port |

**All of them are read-only.** Writing the wrong charge setting can stop charging, or worse.

⚠ The examples have no `#if` guards. The library is named after the board, so it is obvious that
they do not work elsewhere, and guards would clutter every file. Instead `extras/check_examples.sh`
knows to build **a board-only library only on its board** — that is where the constraint belongs.

⚠ On the hardware the **charge current was set to 10 mA** (input limit 500 mA).
The BQ25186 can do up to 1 A, so if charging is slow, this is why.

## 4. ⚠ In the default state no analog pin is left free

AIN0–7 are **fixed to P1.04–P1.07 / P1.11–P1.14** by the chip, and this board uses all eight.

| | GPIO | Used by | Bridge |
|---|---|---|---|
| A0 | P1.04 | **`Serial1` TX** → DAP | **SB9 (fitted)** |
| A1 | P1.05 | **`Serial1` RX** | **SB10 (fitted)** |
| A2 | P1.06 | Serial1 RTS | **SB11** |
| A3 | P1.07 | Serial1 CTS | **SB12** |
| A4 | P1.11 | `PMIC_INT` | **SB1 (fitted)** |
| A5 | P1.12 | **`VBAT_MON`** — battery voltage | **SB4 (fitted)** |
| A6 | P1.13 | SW1 | — |
| A7 | P1.14 | LED4 | — |

**All eight are on the headers, so they can be used by removing the bridge.**
But **do not remove A5** — it is the only way to read the battery voltage.
The variant keeps the names `A0`–`A7` for Adafruit compatibility and warns in comments.

✅ **`SB9`–`SB12` are fitted (confirmed on hardware).** That is why the host sees two serial ports,
and `analogRead(A0..A3)` reads the level the DAP is driving. See §5.

---

## 5. `Serial1` — the second UART, and why there are two USB ports

**The host sees two USB serial ports.** The onboard DAP exposes two CDC interfaces, and on the MCU
side they go to different UARTs. Confirmed on hardware (2026-09-12):

| Host port | Core object | GPIO | Bridge |
|---|---|---|---|
| First | `Serial` | P0.00 TX / P0.01 RX | SB5–SB8 |
| Second | **`Serial1`** | P1.04 TX / P1.05 RX | **SB9–SB12** |

```c
Serial.println("port A");     // comes out of the first port
Serial1.println("port B");    // comes out of the second port
```

With both ports open at once, each received only its own string.
**So `SB9`–`SB12` are fitted.** It uses UARTE20 (P1 = domain 20).

The RTS/CTS flow-control lines are on P1.06 / P1.07, but the core does not use them.

⚠ **The cost: A0–A3 cannot be used.** They are the same four pins. If you need `analogRead`,
remove `SB9`–`SB12`, and the second port dies. Both are on the headers, so the choice is yours.

## 6. Expansion headers

![NU54V-DK pinout](NU54V-DK-pinout.svg)

> The figure is **generated** from the tables below — `extras/gen_pinout_svg.py`.
> If you change the tables, run it again. Do not edit the SVG by hand.
>
> **Power/GND were checked on the hardware** (2026-09-12). The drawing text could not tell the two
> power buses apart, so these positions were left blank for a while:
> P2 1–8 · 13–15 · 18 · 30 and P4 1 · 2 · 13 · 18 · 23 · 24 · 28 are GND.
>
> ⚠ **P4 14 · 15 have pads but are not connected** — `SB20`/`SB21` not fitted
> (§7). They are shown as white cells in the figure.
>
> The figure is drawn **the way the board sits** — `P4` on the left (pin 1 at the top),
> `P2` on the right (pin 30 at the top). The tables below are in number order, so the order differs.


> ⚠ The tables below were **extracted from coordinates** in the schematic. P4 matches the drawing
> image on all 18 rows and is verified; P2 is the result of applying the same rule.
> **They have not been checked against the silkscreen on the board.**

### P2 header (30 pins)

| Pin | Name | GPIO | | Pin | Name | GPIO |
|---|---|---|---|---|---|---|
| 9 | A3 / Serial1 CTS | P1.07 | | 20 | — | P2.06 |
| 10 | A2 / Serial1 RTS | P1.06 | | 21 | LED3 / SWO | P2.07 |
| 11 | A1 / Serial1 RX | P1.05 | | 22 | PMIC PG | P2.08 |
| 12 | A0 / Serial1 TX | P1.04 | | 23 | LED1 | P2.09 |
| 16 | SW3 | P1.08 | | 24 | PMIC CE | P2.10 |
| 17 | SPI MISO | P2.04 | | 25 | Serial TX | P0.00 |
| 19 | SPI SS | P2.05 | | 26 | Serial RX | P0.01 |

Numbers not in the table: **1–8 · 13–15 · 18 · 30 = GND**, 29 = `VDD_MOD`,
27 = `SWDCLK`, 28 = `SWDIO`.

### P4 header (30 pins)

| Pin | Name | GPIO | | Pin | Name | GPIO |
|---|---|---|---|---|---|---|
| 4 | Serial CTS | P0.02 | | 12 | LED4 / A7 | P1.14 |
| 5 | Serial RTS | P0.03 | | 16 | Qwiic SDA | P1.02 |
| 6 | SW4 | P0.04 | | 17 | Qwiic SCL | P1.03 |
| 7 | SW2 | P1.09 | | 19 | — | P2.00 |
| 8 | LED2 | P1.10 | | 20 | SPI SCK | P2.01 |
| 9 | A4 / PMIC INT | P1.11 | | 21 | SPI MOSI | P2.02 |
| 10 | A5 / VBAT_MON | P1.12 | | 22 | — | P2.03 |
| 11 | SW1 / A6 | P1.13 | | | | |

3 = `MOD_RST`, **1 · 2 · 13 · 18 · 23 · 24 · 28 = GND**,
25 = `VDD_MOD`, 26 = `VDD_3V3_SYS` (3.3V output), 27 = `VBAT`, 29 = `VEXT`,
30 = `VBUS` (5–14V input).

⚠ **P4 14 · 15 are printed `P1.00` / `P1.01` on the header but do not reach the MCU.**
For those pins to reach the header `SB20` / `SB21` would have to be fitted, and **on the board they
were confirmed not fitted** (2026-09-12). P1.00 / P1.01 are crystal-only right now. See §7.

---

## 7. Clock — the crystal is hard-wired

The 32.768 kHz crystal Y1 is fitted on P1.00 (XL1) / P1.01 (XL2), with load caps
C1/C2 = 13 pF. → `USE_LFXO`.

Four bridges decide where these two pins go:

| | What | On the board |
|---|---|---|
| `SB18` / `SB19` | P1.00 / P1.01 ↔ **crystal** | Fitted (BLE works) |
| `SB20` / `SB21` | P1.00 / P1.01 ↔ **P4 header** | **Not fitted (confirmed)** |

→ **P1.00 / P1.01 cannot be used as GPIO and do not appear on the header.**
Both are blocked with `NRF54L_PIN_NC` in the variant's pin map.

⚠ Conversely, fitting `SB20`/`SB21` and removing `SB18`/`SB19` disconnects the crystal.
You would then have to build with `USE_LFRC`, and **it cannot be used for BLE**
(accuracy +9000 ppm — CLAUDE.md §7 F12).

## 8. ⚠ `J1` — current measurement jumper

**A 2-pin header sits in series between the LDO output and the module supply.**

```
VSYS ─ U1 (TPS7A3701) ─ VDD_3V3_SYS ─[ J1 ]─ VDD_MOD ─ module
```

| | |
|---|---|
| Jumper fitted (default) | Normal operation |
| Jumper removed, ammeter across the two pins | Measures **only the module rail** current |

The LDO feedback is `R5` 52.3K / `R6` 30.1K, so about **3.3 V** (`3.3V OUT` on P4 pin 26).

**The last open M1 item is the current measurement** (CLAUDE.md, with the probe disconnected — §7 F8).
This board comes with a header for exactly that, so measure here.
The placement is good too — **the DAP (`VDD_3V3_DAP`), the PMIC and the charge LED are upstream of J1**
and do not get mixed in.

⚠ However `VDD_MOD` is not MCU-only. These are also on it:

- **Qwiic connector J5-2** — a plugged-in sensor is measured too
- Qwiic pull-ups `R29`/`R30` (only while the bus is LOW)
- The VCC of LED buffers `U8`/`U9`, reset pull-up `R39`

**Unplug Qwiic when chasing µA.**

---

## 9. Not yet confirmed

- [ ] Check the P2 header pin numbers against the silkscreen (P4 is verified against the drawing image)
- [x] `SB5`–`SB8` — fitted, since `Serial` works


- [ ] Remove `J1` and measure the actual current (M1 closing item)
- [ ] Whether the power-sheet LEDs `D3`/`D5` light on battery too — if always on,
      ~2 mA at 1 kΩ would make the µA design above pointless

**Confirmed**: `SB1`–`SB4` fitted and settled as INT/PG/CE/VBAT_MON,
`SB20`/`SB21` not fitted (crystal fixed),
`SB14`/`SB15` fitted (Qwiic works on hardware), `SB9`–`SB12` fitted (`Serial1`),
the PMIC is 0x6A on the `Wire` bus.
