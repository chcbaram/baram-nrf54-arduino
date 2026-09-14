# XIAO nRF54L15 / XIAO nRF54L15 Sense (Seeed Studio)

*[English](XIAO-nRF54L15.md) · [한국어](XIAO-nRF54L15.ko.md)*

Analysis of schematic `XIAO nRF54L15.kicad_sch` (Rev V0.9, 2025-05-29, CC BY-SA 4.0).
MCU: **nRF54L15-CAAA-R** (WLCSP).

The pin assignment was **checked line by line against the upstream Zephyr board definition and matches**
(`zephyrproject-rtos/zephyr` `boards/seeed/xiao_nrf54l15/`).
Where the schematic alone is ambiguous (LFXO load capacitance, the D6/D7 UART instance), that definition is authoritative.

| | Value |
|---|---|
| Chip | nRF54L15 (1.5 MB RRAM / 256 KB RAM) |
| FQBN | `baram-nrf54:nrf54l:xiao_nrf54l15` |
| variant | `xiao_nrf54l15` |
| Memory layout | Same as NU54V-DK → the nRF54L15 section of [MEMORY-MAP.md](../MEMORY-MAP.md) |

Fitted chip check (FICR, measured):

```
INFO.PART    @ 0x00FFC31C  0x00054B15   nRF54L15
INFO.PACKAGE @ 0x00FFC324  0x00004341   "CA"
INFO.RAM     @ 0x00FFC328  0x00000100   256 KB
INFO.RRAM    @ 0x00FFC32C  0x000005F4   1524 KB
```

> **The only difference from the Sense model is whether the IMU / PDM microphone are fitted.** The pin map,
> memory and boot path are the same, so they share the board entry and the variant. On a model without
> sensors, switching `PIN_SENSOR_POWER` on does nothing.

---

## 1. How it differs from the NU54-DK (read first)

| | NU54-DK | **XIAO nRF54L15** |
|---|---|---|
| Debug probe | None. External CMSIS-DAP needed | **Onboard ATSAMD11D14A** |
| `Serial` | UARTE30, P0.00/P0.01, CP2102N | **UARTE20, P1.09/P1.08, the same SAMD11** |
| LEDs | 4, **active HIGH** | 1, **active LOW** |
| Buttons | 4, no external pull-up | 1, **external 10K pull-up** |
| LFXO load caps | External 13 pF (C13/C14) | **None → internal 16 pF** |
| Analog | A0–A7 | **A0–A3** (D4 has no AIN) |
| Antenna | — | **Goes through an RF switch.** It has to be powered |

---

## 2. Debug / upload — one cable

The USB-C D+/D− go straight to the **ATSAMD11D14A (U4)**, and the SAMD11 drives the nRF54L15 SWD
through level shifter U12 (UM3501D4).
Reset is also controlled by the SAMD11: PA05 → `nRF54_RST_CTL` → Q2.

The same SAMD11 also acts as a **USB CDC serial bridge** (PA08/PA09 ↔ P1.08/P1.09).
So **upload and `Serial` work over one cable at the same time.** No external probe is needed.

Measured:

```
$ probe-rs list
[0]: Seeed Studio XIAO nrf54 CMSIS-DAP -- 2886:0066-1:5784477E (CMSIS-DAP)

Serial port: /dev/cu.usbmodem<serial>3
Upload: writing + verifying a 38 KB hex in 1.9 s
```

### Test points on the back

| TP | Signal | TP | Signal |
|---|---|---|---|
| TP1 | nRF SWCLK | TP5 | VSYS_3V3 |
| TP2 | nRF SWDIO | TP6 | SAMD11 RESET |
| TP3 | GND | TP7 | SAMD11 SWCLK |
| TP4 | nRF RESET | TP8 | SAMD11 SWDIO |

TP1–TP4 let you **attach an external probe directly.** So there is a recovery path even if the SAMD11
firmware breaks. TP6–TP8 are the path for reflashing the SAMD11 itself.

---

## 3. Pin assignment

### LED — **active LOW** (opposite of the NU54-DK)

| Part | Pin | Circuit |
|---|---|---|
| D3 green (USR_LED) | **P2.00** | `VSYS_3V3 ─ R15 1.5K ─ D3 ─ P2.00` |
| D2 red (Charge) | — | Driven directly by the charger IC. **The MCU cannot control it** |

The pin is on the cathode side, so LOW turns it on → `LED_STATE_ON = 0`.
Zephyr has the same: `gpios = <&gpio2 0 GPIO_ACTIVE_LOW>`.

### Button — external pull-up

| Part | Pin | Notes |
|---|---|---|
| K2 (USR_KEY) | **P0.00** | R13 10K pull-up + C24 100nF. active LOW |
| K1 | RESET | R12 10K pull-up. Cannot be read by the MCU |

Unlike the NU54-DK there is an external pull-up, so `INPUT` alone works.
The variant still uses `INPUT_PULLUP` (in parallel, harmless).

### UART — through the onboard SAMD11

| nRF54L15 | Direction | SAMD11 |
|---|---|---|
| **P1.09** | TX → | PA09 (`SAMD11_RX`) |
| **P1.08** | ← RX | PA08 (`SAMD11_TX`) |

The net names are from the SAMD11's point of view, which is easy to confuse. `SAMD11_TX` is the line the
SAMD11 **sends** on, so on the nRF side it is RX. Zephyr `uart20_default` has the same assignment
(`UART_TX = 1,9` / `UART_RX = 1,8`). There is no flow-control wiring.

**The instance is UARTE20** — different from UARTE30 on the NU54-DK,
because P1 is domain 20 ([PERIPHERAL-PINMAP.md](../PERIPHERAL-PINMAP.md)).
This is why the core takes the vector from the variant
(`SERIAL_UARTE_IRQ_HANDLER`, CLAUDE.md §7 F10 ③).

### XIAO header (14 pins)

| Pin | Name | GPIO | | Pin | Name | GPIO |
|---|---|---|---|---|---|---|
| 1 | D0 / A0 | P1.04 | | 14 | 5V | VBUS |
| 2 | D1 / A1 | P1.05 | | 13 | GND | |
| 3 | D2 / A2 | P1.06 | | 12 | 3V3 | VSYS_3V3 |
| 4 | D3 / A3 | P1.07 | | 11 | D10 / MOSI | P2.02 |
| 5 | D4 / SDA | P1.10 | | 10 | D9 / MISO | P2.04 |
| 6 | D5 / SCL | P1.11 | | 9 | D8 / SCK | P2.01 |
| 7 | D6 / TX | P2.08 | | 8 | D7 / RX | P2.07 |

Names off the header also match the Zephyr connector definition:
`D11 = P0.03`, `D12 = P0.04`, `D13 = P2.10`, `D14 = P2.09`, `D15 = P2.06`.

> ⚠ **D7 (P2.07) is shared with SWO.** With SWO tracing on, this pin cannot be used.

> **`Serial1` (UARTE21) on D6 / D7 is within spec** — confirmed 2026-09-12.
> For a while this document said "it breaks the domain rule", but **what was under-specified was the rule.**
> Pin Planner's SoC definition states `UARTE21.TXD → P2.08`, `UARTE21.RXD → P2.07`
> ([PERIPHERAL-PINMAP.md](../PERIPHERAL-PINMAP.md) §1·§4).
> The schematic and Zephyr were right. It just has not been assigned yet (M2).

### Clock — two crystals, **no external load caps**

| | Part | Pins |
|---|---|---|
| LFXO | X2 32.768 kHz ±20 ppm | P1.00 (XL1) / P1.01 (XL2) |
| HFXO | X1 32 MHz ±10 ppm | XC1 / XC2 |

**⚠ This is the most important item on this board.** The NU54-DK has 13 pF external caps (C13/C14)
next to the crystal, but **this board does not.** It is designed to use the chip's internal caps.

If the internal caps are not set (= writing 0 to `INTCAP`), the load capacitance is too low and the
oscillator runs fast. Measured:

| Setting | vs host |
|---|---|
| External caps (INTCAP=0) — **wrong** | **+805 ppm** |
| Internal caps 16 pF | **-14 ppm** |

+805 ppm is far beyond the BLE requirement of ±250 ppm, so **connections drop in M3.**
Yet at M1 there is no symptom at all — `millis()` and `micros()` agree perfectly and the tick is
exact, so **it only shows up against a host clock.**
It is the same family of trap as CLAUDE.md §7 F12.

16000 fF was taken from the Zephyr board definition:

```dts
&lfxo {
	load-capacitors = "internal";
	load-capacitance-femtofarad = <16000>;
};
```

> **Do not hard-code the register value.** The `INTCAP` calculation uses `FICR.XOSC32KTRIM`, and that
> trim **differs from chip to chip.** The variant gives only the capacitance (fF) and the core
> calculates at run time (`lfxo_intcap_calc()` in `port_grtc.c`).
> This unit has `XOSC32KTRIM = 0x013D0015` (SLOPE 21, OFFSET 317) → `INTCAP = 21`.
>
> Do not use the nrfx `NRF_OSCILLATORS_LFXO_CAP_CALCULATE` macro.
> It computes `((SLOPE + 392) >> 9) * (cap*2-12)`, so the first term truncates to 0 and **the result is
> the same regardless of cap** (4 for 6/7/9/11 pF on this chip).

### Analog (SAADC)

| Name | Pins | AIN |
|---|---|---|
| A0 – A3 | P1.04 – P1.07 | AIN0 – AIN3 |

**⚠ Different from the XIAO nRF52840. There is no A4 / A5.** By XIAO convention A4/A5 are D4/D5,
but D4 (P1.10) has no AIN. The pins with AIN are only P1.04–07 and P1.11–14,
and P1.11–14 are already used by SCL / microphone / battery sense.

### Battery voltage

```
VBAT ─ TPS22916(U2) ─ R5 10K ─┬─ P1.14 / AIN7
       EN = P1.15             └─ R6 10K ─ GND
```

The divider path is gated by a load switch, so normally there is no leakage.
Raise `PIN_VBAT_ENABLE` (P1.15) HIGH only while reading.
**Battery voltage = measured voltage × 2.0** (stated on the schematic).

The charger IC is SGM40567-4.2, `iCharge = 24000 / R4(120K) = 200 mA`.

### Onboard sensors (Sense model)

The supply is behind TPS22916 (U11) and **`P0.01` is EN** (0: off, 1: on).
Keeping it off when unused helps low power. The variant leaves it off by default.

| Part | Interface | Pins |
|---|---|---|
| IMU LSM6DS3TR-C (address **0x6A**) | I2C — TWIM30 | SDA P0.04 / SCL P0.03, INT1 P0.02 |
| PDM microphone MSM261DGT006 | PDM20 | CLK P1.12 / DATA P1.13 |

The IMU I2C is a **different bus** from the header `Wire` (TWIM22). Pull-ups R18/R19 4.7K.

### Antenna — the RF switch must be switched on

```
RADIO ─ FM8625H(U10) ─┬─ RF1 ─ ANT1  chip antenna (KH5220-A36)
                      └─ RF2 ─ ANT2  u.FL connector
        VDD  = P2.03 (RF_SW_PWR)
        VCTL = P2.05 (RF_SW_CTL)   0: RF1, 1: RF2
```

**Without power on the switch there is no RF path.** The variant's
`initVariant()` powers it and selects the onboard chip antenna (RF1).
There is no radio in M1, so it only consumes current — **subtract that share when measuring current.**
Whether RF1 really is the onboard antenna is confirmed in M3.

---

## 4. Power

```
USB-C ─┬─ 5V ─┬─ SGM40567 charger IC ─ VBAT ─┐
       │      ├─ SGM2040-3.3 LDO ─ SAMD11-only 3V3 (250 mA)
       │      └───────────────────────────────┴─ Q1 ─ VBUS ─ TPS62843 buck ─ VSYS_3V3 (600 mA)
       └─ D+/D− ─ SAMD11
```

- **The SAMD11 supply is separate from the system supply** (its own LDO). The debugger lives only on USB
- The system 3V3 is a **DC-DC buck**, not an LDO. Account for its efficiency curve in low-power measurements
- The battery goes to the BAT pad. USB and battery are OR-ed through Q1 (P-MOSFET) + D4

---

## 5. Summary of points for the Arduino core

| # | Item |
|---|---|
| 1 | The LED is **active LOW** → `LED_STATE_ON 0` (opposite of the NU54-DK) |
| 2 | There is **only one** LED. `LED_RED`/`LED_BLUE`/`LED_CONN` are all the same pin |
| 3 | The button **has an external pull-up** |
| 4 | `Serial` = **UARTE20** (P1.09 TX / P1.08 RX). The vector is `SERIAL20_IRQHandler` |
| 5 | **P1.00 / P1.01 are LFXO only** — do not expose them as GPIO |
| 6 | **The LFXO internal cap of 16000 fF is required.** Without it, +805 ppm (§3) |
| 7 | **There is no A4 / A5** — different from the XIAO nRF52840 |
| 8 | The battery voltage reads only with `P1.15` on. ×2.0 |
| 9 | Sensor power is `P0.01`. Off by default |
| 10 | **The RF switch must be on before BLE** (P2.03) |
| 11 | D7 (P2.07) is shared with SWO |
| 12 | `Serial1` (D6/D7, UARTE21) after checking the domain rule (M2) |

## ⚠ Serial input — the VCOM stalls on large bursts

The onboard CMSIS-DAP VCOM is weak in the **host -> target direction**. Push about 1 KB in at once and
receiving dies completely afterwards, and **a target reset does not bring it back.**
Replugging USB restores it. Sending (board -> PC) keeps working meanwhile.

It is a board issue, not a core issue — the same firmware on the NU54-DK (CP2102N) receives 1024 bytes
without loss. Evidence in `docs/STATUS.md` §2.5.

**If serial input suddenly stops, replug USB.** Send long strings in pieces.
