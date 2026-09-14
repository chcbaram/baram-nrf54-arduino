# Peripheral ↔ GPIO power domains (nRF54L15)

*[English](PERIPHERAL-PINMAP.md) · [한국어](PERIPHERAL-PINMAP.ko.md)*

> **This is where nRF52 habits break.** On nRF52 PSEL could connect any GPIO to any peripheral,
> but on nRF54L each peripheral instance belongs to a power domain and can select
> **only pins on the GPIO ports that domain owns.**
>
> The core enforces this rule with `static_assert` in `cores/nrf54l/nrf54l_domains.h`.
> A wrong assignment **fails the build.**

---

## 0. Know this first — the same number is **one block**

This comes before the domain rule. **`SPIM`/`SPIS`/`TWIM`/`TWIS`/`UARTE` with the same number are
the same hardware block in different modes. They cannot be used at the same time.**

The MDK base addresses say so directly (`nrf54l15_global.h`):

| Base | Same block | Port |
|---|---|---|
| `0x5004A000` | SPIM00, SPIS00, UARTE00 | P2 |
| `0x500C6000` | SPIM20, SPIS20, **TWIM20**, TWIS20, UARTE20 | P1 |
| `0x500C7000` | SPIM21, SPIS21, **TWIM21**, TWIS21, UARTE21 | P1 |
| `0x500C8000` | SPIM22, SPIS22, **TWIM22**, TWIS22, UARTE22 | P1 |
| `0x50104000` | SPIM30, SPIS30, **TWIM30**, TWIS30, UARTE30 | P0 |

**Two conclusions follow:**

1. If `Serial` is UARTE30, **TWIM30 / SPIM30 cannot be used** on that board.
   The NU54-DK variant actually had `Serial` (UARTE30) and `Wire` (TWIM30) together —
   a configuration in which `Serial` would have died the moment `Wire` was attached in M2
2. **TWIM00 does not exist.** So **I2C cannot go on P2**

> The vector being named **`SERIAL30_IRQHandler`** rather than `UARTE30_IRQHandler` is for the
> same reason (§7 F10 ③). It is one SERIAL block.

When assigning pins for a new board, **check block collisions with this table first**, then check
the port with the domain rule below.

---

## 1. The rule

**The first digit of the instance number is the domain, and each domain owns one GPIO port.**

| Instance | Domain | GPIO port | Character |
|---|---|---|---|
| `x00` | 00 | **P2** | High speed |
| `x20` `x21` `x22` | 20 | **P1** | Main |
| `x30` | 30 | **P0** | Always on |

Easy to remember: `UARTE30` → P0, `TWIM20` → P1, `SPIM00` → P2.

### ⚠ Exception — SPIM/SPIS 20·21 and UARTE 20·21 can use **specific pins on P2**

The table above is the base rule, but not the whole story. From the Nordic pin planning guide:

> Rule 1: "Generally, peripherals must use pins in their own power domain."
>
> "**Selected pins on P2 can also be used by certain serial interfaces
> (SPIS, UARTE) located in PERI**, although this configuration is less
> power-efficient."

**Which pins the "selected pins" are was settled on 2026-09-12.** The source is the SoC definition JSON
of the Pin Planner app that Nordic published (§4). The documentation site blocks scripted access, but that
repository is on GitHub and can be read directly.

| Instance | Signal | Usable P2 pins |
|---|---|---|
| **SPIM/SPIS20** | SCK / SDO / SDI / CS / DCX | P2.01 / P2.02 / P2.04 / P2.05 / P2.00 |
| **SPIM/SPIS21** | SCK / SDO / SDI / CS / DCX | P2.06 / P2.08 / P2.09 / P2.10 / P2.07 |
| **UARTE20** | TXD / RXD / CTS / RTS | P2.02 / P2.00 / P2.04 / P2.05 |
| **UARTE21** | TXD / RXD / CTS / RTS | P2.08 / P2.06 / P2.09 / P2.10 |

**Three things came out of it:**

1. **"SPIS" in the guide is too narrow — SPIM works too.** The JSON defines `SPIM/SPIS20` as one group
   and allows P2 pins. The `chcbaram/nu54dk` firmware using SD on P2 as `&spi20` (master)
   was **a configuration within spec**
2. **Number 22 is not an exception.** SPIM/SPIS22·UARTE22 are P1-only
3. **There is no exception for I2C.** TWIM/TWIS 20·21·22·30 all use only their own domain's port.
   Combined with the absence of TWIM00, **I2C on P2 is impossible in any way**

The high-speed domain SPIM00's P2 pins are (all `driveStrengthRequirement: extra high`):

| Signal | Pins |
|---|---|
| SCK / SDO / SDI / CS / DCX | P2.01·P2.06 / P2.02·P2.08 / P2.04·P2.09 / P2.05·P2.10 / P2.00·P2.07 |

Our variant's SPI assignment (SCK P2.01 / MOSI P2.02 / MISO P2.04) fits this
and works on hardware (`docs/STATUS.md` §2.12).

⚠ **The L05 and L15 constraints are identical** — the two Pin Planner definitions were compared
programmatically (31 peripherals, no difference). This agrees with them being the same binned die.

`nrf54l_domains.h` enforces the base rule, and boards that use the exception **state it with a dedicated
macro** (`NRF54L_ASSERT_PERI_SERIAL_PIN`). That way the code shows exactly where the exception is used.

Other clauses of this guide are recorded here too:

> Rule 2: "Some peripherals with clock signals (like SPI, TWI, and TRACE) require
> the use of specific dedicated clock pins."
>
> Rule 4: peripherals that use only dedicated pins — FLPR, SPIM00/UARTE00, GRTC, TAMPC, NFC,
> RADIO direction-finding.

### Basis

The MDK peripheral base addresses cluster by domain
(sorting `NRF_*_S_BASE` in `nrf54l15_global.h` gives this directly):

```
0x50040000  AAR00 CCM00 CRACEN DPPIC00 ECB00 KMU MPC00 PPIB00 PPIB01
            RRAMC SPIM00 SPIS00 SPU00 UARTE00 VPR00
0x50050000  CTRLAP GPIOHSPADCTRL  P2  TAD TIMER00
0x50080000  DPPIC10 EGU10 PPIB10 PPIB11 RADIO SPU10 TIMER10     ← SoftDevice only
0x500C0000  DPPIC20 EGU20 MEMCONF PPIB20-22 SPIM20-22 SPIS20-22
            SPU20 TIMER20-24 TWIM20-22 TWIS20-22 UARTE20-22
0x500D0000  GPIOTE20 I2S20 NFCT  P1  PDM20 PDM21 PWM20-22 SAADC TAMPC TEMP
0x500E0000  GRTC QDEC20 QDEC21
0x50100000  CLOCK COMP DPPIC30 GPIOTE30 LPCOMP  P0  POWER PPIB30
            RESET SPIM30 SPIS30 SPU30 TWIM30 TWIS30 UARTE30 WDT30 WDT31
```

**The schematic confirms this three times independently:**

1. **SAADC is in the P1 domain** → AIN0–7 are all P1.04–07, P1.11–14. No other port has analog inputs
2. **NFCT is in the P1 domain** → NFC1/NFC2 are P1.02/P1.03
3. **UARTE30 is in the P0 domain** → the console UART is P0.00–03 (confirmed on hardware)

---

## 2. Peripherals by domain

### Domain 00 → **P2** (P2.00–P2.10 exposed on the NU54-DK)

| Peripheral | Notes |
|---|---|
| **SPIM00 / SPIS00** | The only high-speed SPI. Arduino `SPI` is here |
| UARTE00 | |
| TIMER00 | Usable by the application |
| GPIOHSPADCTRL | High-speed pad control. Needed for high-speed signals |

> ⚠ High-speed signals may need `OUTPUT_H0H1` or the nRF54L-specific `OUTPUT_E0E1` drive.
> For P2 high-speed routing, HSBIAS slew and the SPIM anomaly 8 workaround, see CLAUDE.md §4.

### Domain 20 → **P1** (P1.00–P1.14 exposed on the NU54-DK)

| Peripheral | Instances |
|---|---|
| SPIM / SPIS | 20, 21, 22 |
| TWIM / TWIS | 20, 21, 22 |
| UARTE | 20, 21, 22 |
| PWM | 20, 21, 22 |
| **SAADC** | 1 (AIN0–7 must be on P1) |
| PDM | 20, 21 |
| I2S | 20 |
| QDEC | 20, 21 |
| GPIOTE | 20 |
| NFCT | 1 |
| TIMER | 20–24 |

### Domain 30 → **P0** (P0.00–P0.04 exposed on the NU54-DK)

| Peripheral | Notes |
|---|---|
| UARTE30 | **`Serial`** (P0.00 TX / P0.01 RX) |
| TWIM30 / TWIS30 | **`Wire`** (P0.02 SDA / P0.03 SCL) |
| SPIM30 / SPIS30 | |
| GPIOTE30 | `attachInterrupt` on P0 pins |
| COMP / LPCOMP | |
| WDT30 / WDT31 | |

> P0 is the always-on domain, so it stays alive in low-power states.
> Good for wake-up sources.

### No GPIO

`GRTC` (FreeRTOS tick), `RADIO`/`TIMER10`/`EGU10` (SoftDevice only, `nrf_sd_def.h`),
`CRACEN`, `RRAMC`, `MEMCONF` and so on.

---

## 3. Per-board assignment

The pin evidence is in each board document (`docs/boards/`). Here we only check **that it follows the domain rule**.

### NU54-DK / NU54V-DK

| Arduino function | Instance | Pins | Domain |
|---|---|---|---|
| `Serial` | UARTE30 | P0.00 TX / P0.01 RX | 30 → P0 ✅ |
| `Wire` | **TWIM22** | **P1.11 SDA / P1.12 SCL** | 20 → P1 ✅ |
| `SPI` | SPIM00 | P2.01 SCK / P2.02 MOSI / P2.04 MISO / P2.05 SS | 00 → P2 ✅ |
| `analogRead` | SAADC | A0–A7 = P1.04–07, P1.11–14 | 20 → P1 ✅ |
| `analogWrite` | PWM20–22 | P1.xx (assigned in M2) | 20 → P1 |
| `attachInterrupt` | GPIOTE20 / GPIOTE30 | P1 / P0 — **not P2** | — |

### XIAO nRF54L15 / Sense

Basis: `docs/boards/XIAO-nRF54L15.md`. The schematic and the Zephyr board definition agree.

| Arduino function | Instance | Pins | Domain |
|---|---|---|---|
| `Serial` (onboard USB CDC) | UARTE20 | P1.09 TX / P1.08 RX | 20 → P1 ✅ |
| `Wire` (header D4/D5) | TWIM22 | P1.10 SDA / P1.11 SCL | 20 → P1 ✅ |
| `Wire1` (onboard IMU) | TWIM30 | P0.04 SDA / P0.03 SCL | 30 → P0 ✅ |
| `SPI` (header D8/D9/D10) | SPIM00 | P2.01 SCK / P2.02 MOSI / P2.04 MISO | 00 → P2 ✅ |
| `analogRead` | SAADC | A0–A3 = P1.04–07 | 20 → P1 ✅ |
| PDM microphone | PDM20 | P1.12 CLK / P1.13 DIN | 20 → P1 ✅ |
| (unassigned) `Serial1` candidate | UARTE21 | P2.08 TX / P2.07 RX | 21 → P2 ✅ **legal as the exception** (§1) |

### Why the NU54-DK `Wire` is on P1

**It was first put on P0.02 / P0.03. That was wrong.**

Those pins are indeed free — they are the CP2102N RTS/CTS, and flow control is not used
(`cores/nrf54l/Uart.h`). But P0 is domain 30, and **TWIM30 is the same block as UARTE30,
which `Serial` uses** (§0). The two cannot be on at the same time.

P2 is no alternative — **TWIM00 does not exist at all.**

So it has to be P1, and P1's 15 pins are already crowded (LFXO 2, NFC 2, AIN 8, buttons 2, LEDs 2).
Of what is left, **the only pair that does not overlap another function is P1.11 / P1.12**.

⚠ **The cost: with `Wire` in use, A4 / A5 cannot be used.** They are the same pins.
If analog matters more to a sketch, just do not use `Wire` (the pins overlap, but
as long as the two are not on at the same time there is no problem).

> The XIAO nRF54L15 does not have this constraint. `Serial` is UARTE20 (`0x500C6000`) and
> `Wire` is TWIM22 (`0x500C8000`), so the blocks differ. Seeed assigned them well.

---

## 4. The exact P2 pin assignment — ✅ settled (2026-09-12)

Two long-open questions, **(a) which pins does the P2 exception apply to** and
**(b) within a domain, which pin can carry which signal**, were **both closed** by the Pin Planner
SoC definition (§7). There was no need to dig through the PS PDF.

### P0 · P1 are the whole port

`allowedGpio` is written as `P0*` / `P1*`. **Any pin on that port works.**
There are no per-pin constraints. So the variant's P1 pin assignment is free as long as the domain is right.

### P2 has **two candidates per signal**

The full table is in §1. Here only the shape — **it is not "any P2 pin".**
`SPIM00.SCK` is **only** P2.01 and P2.06; give it another P2 pin and it silently does not work.

⚠ **P2.03 is dedicated to `sQSPI.D2`, so for us it is a GPIO-only pin.** Neither SPIM nor UARTE
reach it. It is an easy place to assume P2 is continuous.

### Interrupts, PWM and ADC do not exist on P2 at all

| | Ports that work |
|---|---|
| `GPIOTE20` | **P1 only** (8 channels) |
| `GPIOTE30` | **P0 only** (4 channels) |
| `PWM20/21/22` | **P1 only** |
| `SAADC` `AIN0–7` | **fixed to P1.04–P1.07 / P1.11–P1.14** |

**There is no GPIOTE and no PWM for P2.** So P2 pins can have neither
`attachInterrupt()` nor `analogWrite()`. The hardware is missing; it is not the software declining.

⚠ **The NU54-DK `LED_BUILTIN` (`PIN_LED1` = P2.09) and `PIN_LED3` (P2.07) fall here.**
To test PWM or pin interrupts on that board, use LED2 (P1.10) or LED4 (P1.14).

### It is stopped at build time

We do not rely on anyone reading the table. `cores/nrf54l/nrf54l_pinmap.h` has the same constraints
as macros, and the variant checks its assignment against them.

```c
NRF54L_ASSERT_SIG(PIN_SPI_SCK, SPIM00_SCK, "SPI SCK");
```

Give it P2.03 and it stops like this:

```
error: static assertion failed: SPI SCK : SPIM/SPIS00.SCK only works on P2.01, P2.06
```

This is finer than the port-level check in `nrf54l_domains.h`. Looking only at the port, P2.03 passes.

⚠ **Pins have to be passed as macros.** Arduino-style aliases like `static const uint8_t D6`
are **not constant expressions in C** and cannot go into `_Static_assert`. variant.h is also included
from the core's `.c` files, so it is compiled as C too.

### To look at the table again

The example comments in `libraries/PinMap/` hold the full per-chip and per-board tables.
They open directly from **File → Examples → PinMap** in the IDE. Those tables are generated by
`extras/gen_pinmap.py` from the Pin Planner JSON — do not edit them by hand.

## 5. nRF54LM20A — ✅ in the core (2026-09-14)

The address ranges split differently from L15. The two questions below were **resolved on 2026-09-12.**

| Range | LM20A members |
|---|---|
| `0x50040000` | SPIM00 SPIS00 UARTE00 … |
| `0x50050000` | **P2**, TIMER00, EGU00, **USBHS**, GPIOHSPADCTRL |
| `0x500C0000` | SPIM/TWIM/UARTE **20–22**, TIMER20–24 … |
| `0x500D0000` | **P1**, **P3**, GPIOTE20, PWM20–22, SAADC, NFCT … |
| `0x500E0000` | GRTC, QDEC, **SPIM/TWIM/UARTE 23·24**, TDM |
| `0x50100000` | **P0**, SPIM30 TWIM30 UARTE30 GPIOTE30 … |

**Two places where the L15 rule does not simply extend** — ✅ **both confirmed (2026-09-12).**
The basis is Pin Planner's `mcus/nrf54lm20a/fccsp98-3.67x3.85-paaa.json` (§7).

### ① P3 **shares domain 20** with P1

L15's "one GPIO port per domain" widens on LM20A to
**"domain 20 owns P1 and P3"**.

| | P1 | **P3** |
|---|---|---|
| SPIM/SPIS/TWIM/TWIS/UARTE **20–24** | ✅ | ✅ |
| PWM20/21/22 | ✅ | ✅ |
| GPIOTE20 | ✅ | ✅ |
| **SAADC (AIN)** | ✅ | ❌ |
| **PDM20/21, TDM, QDEC20/21** | ✅ | ❌ |

So P3 follows **only up to serial · PWM · interrupts**; analog and audio are P1-only.

### ② SERIAL 23·24 also use P1 and P3

They sit in the `0x500E` range (where GRTC is), but **their GPIO is on the domain 20 side**.
`SPIM/SPIS23`, `TWIM/TWIS23`, `UARTE23`, and 24 as well all have
`allowedGpio` of `P1*` and `P3*`. **Address range and GPIO domain are separate** —
that is why this item was open, and it is now confirmed that the domain must not be guessed from the range.

### P2 is the same as L15

No port-wide rule, pin by pin. On LM20A there are boards where **P2.00–P2.05 are used as `sQSPI`**
(the onboard 8MB flash of the XIAO nRF54LM20A).

### How it went into the core

Handled in two layers.

1. **Port check** — `NRF54L_IS_DOMAIN20_PORT(port)` in `nrf54l_domains.h`.
   On LM20A it is P1 or P3; on other chips, P1. `wiring_interrupt.c` (GPIOTE20) and
   `wiring_analog.c` (PWM) accept P3 through it.
   The SAADC check (`NRF54L_ASSERT_ANALOG_PIN`) stays **P1-only**
2. **Signal-level check** — `nrf54l_pinmap.h` is generated with **two tables** and chooses with
   `#if defined(NRF54LM20A_XXAA)`. The variant uses the same
   `NRF54L_ASSERT_SIG(pin, SPIM23_SCK, ...)` without caring about the chip

⚠ The LM20A JSON lists dedicated non-GPIO pads (`USBHS.D+` / `D-`) in `allowedGpio`
as `"D+"`. The generator once died parsing that as a pin name and left the header **as an empty file** —
it now accepts only GPIO forms and writes the file only after everything is built.

It is also worth recording that USB (USBHS) is in the `0x50050000` range, i.e. **the same high-speed domain as P2**.

---

## 6. When making a new board variant

**Always copy** the domain check block at the end of `variant.h`.
It is the "power domain check" section of `variants/nu54dk/variant.h`.

```c
NRF54L_ASSERT_DOMAIN30_PIN(PIN_SERIAL_TX, "Serial(UARTE30) TX");
NRF54L_ASSERT_SPIM00_PIN(PIN_SPI_SCK,     "SPI SCK");
NRF54L_ASSERT_ANALOG_PIN(PIN_A0,          "A0");
```

A wrong assignment stops the build with an error like this:

```
error: static assertion failed: SPI SCK : SPIM00/UARTE00 can only use pins on P2
```

---

## 7. Where the tables come from — the Pin Planner app's SoC definitions

**https://github.com/NordicPlayground/PinPlanner**

A pin planning web app Nordic published, and **the pin ↔ peripheral constraints are inside as JSON.**
The documentation site (`docs.nordicsemi.com`) and DevZone block scripted access with 403
(`docs/DATASHEETS.md`), but this repository is on GitHub and can simply be downloaded.
**When a pin constraint needs checking, look here before hunting for a PDF.**

```sh
curl -sL https://raw.githubusercontent.com/NordicPlayground/PinPlanner/main/mcus/nrf54l15/qfn48-6x6-qfaa.json
```

| Path | Content |
|---|---|
| `mcus/<soc>/<package>.json` | Pin list (`pins`) and allowed pins per peripheral (`socPeripherals`) |
| `mcus/<soc>/devicetree-templates.json` | Zephyr DTS fragments |
| `devkits/*.json` | Nordic DK board definitions |

`socPeripherals[].signals[].allowedGpio` is the key. `P1*` means the whole port,
and an entry like `P2.01` means only that pin.
`driveStrengthRequirement` is included too (SPIM00 is `extra high`).

Supported SoCs: nRF54L05 / L10 / L15 / LM20A / LS05A·B / LV10A — **covers the LM20A of M6.**
