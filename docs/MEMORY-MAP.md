# Memory map — per chip (nRF54L05 / nRF54L15 / nRF54LM20A)

*[English](MEMORY-MAP.md) · [한국어](MEMORY-MAP.ko.md)*

**This document is per chip.** The fitted chip decides the layout, not the board.
For board pin maps see `docs/boards/`.

| Board | Chip | Section to use |
|---|---|---|
| NU54-DK | nRF54L05 | nRF54L05 section |
| NU54V-DK | nRF54L15 | nRF54L15 section |
| XIAO nRF54L15 / Sense | nRF54L15 | nRF54L15 section (same as NU54V-DK) |
| XIAO nRF54LM20A / Sense | **nRF54LM20A** | nRF54LM20A section |

NU54-DK and NU54V-DK have identical schematics and pin maps; only the fitted module differs.
Only the memory layout differs, so the linker script and SoftDevice hex are kept separate.

| | NU54-DK | NU54V-DK |
|---|---|---|
| Chip | **nRF54L05** | nRF54L15 |
| FQBN | `baram-nrf54:nrf54l:nu54dk` | `...:nu54vdk` |
| RRAM | 500 KB | 1.5 MB |
| RAM | 96 KB | 256 KB |
| Linker | `nrf54l05_s145_v10.ld` | `nrf54l15_s145_v10.ld` |
| SD hex | `s145_nrf54l05_...` (@0x5A800) | `s145_nrf54l15_...` (@0x15A800) |

> ⚠ **The nRF54L05 is a binned nRF54L15 die.**
> The out-of-spec RRAM/RAM physically exists, so flashing an L05 with L15 settings
> **simply works, with no error.** It actually ran that way through all of M1 with no symptom.
> Picking the wrong board silently goes out of spec, so check the fitted chip with FICR:
>
> ```
> FICR INFO.PART     @ 0x00FFC31C   0x00054B05 = L05 / 0x00054B15 = L15
> FICR INFO.VARIANT  @ 0x00FFC320   ASCII (feature variant + HW revision)
> FICR INFO.PACKAGE  @ 0x00FFC324   ASCII (package code, e.g. "CA")
> FICR INFO.RAM      @ 0x00FFC328   in KB. 0x60 = 96 KB (L05) / 0x100 = 256 KB (L15)
> FICR INFO.RRAM     @ 0x00FFC32C   in KB. 0x1F4 = 500 KB (L05) / 0x5F4 = 1524 KB (L15)
> ```
>
> nRF54LM20A measured (XIAO): PART `0x054BC20A` · PACKAGE `"PA"` · RAM `0x200` · RRAM `0x7F4`.
> The PART format differs from L05/L15. **RAM `0x200` (512 KB) is rounded up** — see the nRF54LM20A section below.
>
> ⚠ These offsets were originally written with RAM at `0x00FFC324` and `CODESIZE` at `0x00FFC328`,
> **4 bytes early each.** Those addresses are actually `PACKAGE` / `RAM`, so reading them as written
> gives nonsense. The basis is MDK `NRF_FICR_INFO_Type` (FICR base `0x00FFC000` + `INFO` `0x300`),
> and the values above were confirmed by reading the hardware.
> The last field is also named **`RRAM`**, not `CODESIZE`.

---


Source: `nrfconnect/sdk-nrf-bm` v2.0.1
`boards/nordic/bm_nrf54l15dk/bm_nrf54l15dk_nrf54l15_cpuapp_s145_softdevice.dts`
(and `..._s115_softdevice.dts` in the same directory)

No MCUboot / no TrustZone (secure-only, R5).

---

## NU54V-DK — nRF54L15 + S145 v10.0.1

### RRAM (1.5 MB = 0x17D000)

| Start | Size | Region | Notes |
|---|---|---|---|
| `0x00000000` | 1378 KB (`0x158800`) | **application (slot0)** | Includes the vector table |
| `0x00158800` | 4 KB (`0x1000`) | peer_manager | BLE bonding storage (M3) |
| `0x00159800` | 4 KB (`0x1000`) | storage0 | User storage |
| `0x0015A800` | 137 KB (`0x22400`) | **SoftDevice S145** | Ends at `0x17CC00` |

### RAM (256 KB = 0x40000)

| Start | Size | Region |
|---|---|---|
| `0x20000000` | `0x8000` (32 KB) | **SoftDevice** |
| `0x20008000` | `0x38000` (229,376 B) | **application** |

> **Reserved more generously than the DTS default (`0x4780`, 18.25 KB).** The reason is **concurrent connections**.
> The SoftDevice uses more RAM per link, and how much is known only from the value
> `sd_ble_enable()` returns.

**Measured (XIAO nRF54L15, MTU 247, notify queue 1):**

| Concurrent links | Required app RAM start | Within linker `0x20008000` |
|---|---|---|
| 1 | `0x20003750` | ✅ |
| 2 | `0x200046D8` | ✅ |
| 3 | `0x20005668` | ✅ |
| 4 | `0x200065F8` | ✅ |
| 5 | `0x20007590` | ✅ |

About **3980 B** per link (MTU 247). It shrinks with MTU — about 1950 B at MTU 23,
so the same boundary would fit 8 links. **Connection count and MTU trade against each other.**

**The current configuration is peripheral 4 + central 1 + notify queue 3, requiring
`0x20007F48` (measured).** 184 B are left inside the `0x20008000` boundary.

For reference, the Adafruit nRF52840 core reserves `0x20006000` (24 KB). That would allow 3 links.
Reserving 32 KB of 256 KB costs 6.25 % of app RAM (243,328 → 229,376 B).

⚠ More links share radio time, so **per-connection throughput drops.**
`SD_BLE_EVENT_LENGTH` (currently 6 = 7.5 ms) may need to be considered too; that is a measurement item.

#### The notify queue depth uses the same RAM

`SD_BLE_HVN_TX_QUEUE_SIZE` (`BLE_CONN_CFG_GATTS.hvn_tx_queue_size`, default 1)
**sets throughput**. `HVN_TX_COMPLETE` arrives after the packet is sent and ACKed —
near the end of the connection event — so with a queue of 1 you must wait for that completion
before queueing the next one. Even if the peer would take several per event, we can fill only one.
(On iOS at 15 ms interval · MTU 185 that caps at about 97 kbps.)

**Measured (XIAO nRF54L15, 5 links, MTU 247):**

| Queue depth | Required app RAM start | Increase |
|---|---|---|
| 1 | `0x20007590` | — |
| 2 | `0x20007A78` | +1,256 B |
| 3 | `0x20007F68` | +1,264 B |
| 4 | `0x20008450` | +1,256 B |

One slot is **about 251 B per link** (≈ MTU size). So roughly:

```
required RAM(links L, queue Q) ≈ required RAM(L, 1) + (Q - 1) × 251 × L
```

Checked on hardware: 4 links · queue 3 → predicted `0x20006DD0`, measured **`0x20006DD8`** (8 B off, `begin=1`).

**So link count and queue depth trade against each other.** With the chosen 32 KB reservation:

| Goal | Combination | Within the 32 KB reservation (`0x20008000`) |
|---|---|---|
| **Chosen** | **peripheral 4 + central 1 · queue 3** | ✅ `0x20007F48`, 184 B to spare — queue depth matches Adafruit `BANDWIDTH_MAX` |
| Smaller reservation | peripheral 3 + central 1 · queue 3 | ✅ `0x20006DC0` — fits in 28 KB |
| Deeper queue | 4 links · queue 4 | ✅ `0x200071C4` |

#### Role allocation happens **at run time**

The arguments of `Bluefruit.begin(prph, central)` go straight through to `sd_ble_cfg_set()`.
The linker fixes **only the RAM boundary**; how it is split inside is up to the sketch
(same as Adafruit). The `-DSD_BLE_*_LINK_COUNT` in `boards.txt` is **only the default for
`begin()` without arguments**, not an upper limit.

**Measured (XIAO nRF54L15, 32 KB reservation = `0x20008000`, MTU 247 · queue 3):**

| begin(prph, central) | Required app RAM start | |
|---|---|---|
| `(4, 0)` | `0x20006DD8` | ✅ the peripheral side of the board default |
| `(0, 4)` | `0x20006868` | ✅ central is cheaper per link |
| `(2, 2)` | `0x20006C00` | ✅ |
| `(0, 5)` | `0x20007828` | ✅ |
| `(1, 1)` | `0x20004AB0` | ✅ |
| `(8, 0)` | `0x2000B3F8` | ❌ `begin()` returns false |
| `(6, 2)` | `0x2000B220` | ❌ `begin()` returns false |

When RAM is short it **fails instead of silently cutting back.** `sdLastError()` gives `0x04`
(`NRF_ERROR_NO_MEM`), and `ram_required` from `sdCfgResults()` says **how much was needed**.
Raise the linker `RAM ORIGIN` to at least that value.

Thanks to this, the allocations the upstream examples ask for work as is — `central_bleuart_multi`
uses `begin(0, 4)`, and `dual_bleuart` and `rssi_proximity_central` use `begin(1, 1)`.

#### Peripheral and central **cannot have separate buffers**

The SoftDevice allocates buffers per **connection configuration (`conn_cfg_tag`)**, not per role.
But **S145 v10.0.1 allows only one connection configuration** — trying to create a second tag makes
`sd_ble_cfg_set()` return `NRF_ERROR_NOT_SUPPORTED` (6) (confirmed on hardware).
From `ble.h`: *"A second connection configuration (conn_cfg_tag) is attempted to be created."*

So the approach Adafruit uses on nRF52 — splitting `CONN_CFG_PERIPHERAL` / `CONN_CFG_CENTRAL`
to give peripheral MTU 247 and central 23 — **does not work here.**
Both roles share MTU · event length · notify queue, and `conn_count` covers the sum of both.

**Measured (XIAO nRF54L15, MTU 247):**

| periph + central | Queue | Required app RAM start | Boundary `0x20008000` |
|---|---|---|---|
| 4 + 0 | 3 | `0x20006DD8` | ✅ 552 B to spare |
| 3 + 1 | 3 | `0x20006DC0` | ✅ (this combination with a 28 KB reservation) |
| 2 + 1 | 3 | `0x20005C38` | ✅ |
| 3 + 1 | 1 | `0x200065E0` | ✅ |
| 4 + 1 | 1 | `0x20007570` | ✅ |
| **4 + 1** | **3** | **`0x20007F48`** | ✅ 184 B to spare — **chosen** |

**The cost attaches to the number of links, not to the role.** Turning a peripheral into a central
is actually 24 B cheaper (a central has no advertising state).

The reservation was grown from 28 KB to 32 KB to fit `4+1`. App RAM went 233,472 -> 229,376 B
(1.6 % of 256 KB). `3+1` would fit in 28 KB, but multi-peripheral drops from 4 -> 3.

The nRF54L05 shows the same property in measurement: `periph 1 + central 1` = `0x20004AB0`,
24 B cheaper than `periph 2 + central 0` (`0x20004AC8`). The L05 grew its reservation
to have both (see the L05 section below).

⚠ The core default is **1** (same as the Adafruit default). Boards raise it with
`-DSD_BLE_HVN_TX_QUEUE_SIZE=N` in `build.extra_flags` of `boards.txt`.
The nRF54L15 boards set 3; the nRF54L05 keeps the default. To let the sketch choose as upstream does,
`configPrphConn()` would have to actually work, and for that these values must reach
`sd_ble_cfg_set()` inside `sdEnable()` (today the arguments are accepted and discarded).
When RAM is short, `begin()` **must fail rather than silently cut back** — the same rule as link count.

> ⚠ If changing the configuration leaves RAM short, `sd_ble_enable()` returns **the exact address needed**.
> Read it with `sdCfgResults()` and fix **all three together**: the linker script `RAM ORIGIN`/`LENGTH`
> and `upload.maximum_data_size` in `boards.txt`.

> **✅ Confirmed on hardware 2026-09-05** (when the reservation was `0x4780`):
> `.vectors @ 0x00000000`, `.text @ 0x00000E08`, `.data @ 0x20004780`,
> `__StackTop = 0x20040000`. The layout rule is unchanged; only the start address moved up.
> Log: [HIL/M1-nu54dk.md](HIL/M1-nu54dk.md) (Korean)

### Linker script values

```
FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x158800
RAM   (rwx) : ORIGIN = 0x20008000, LENGTH = 0x38000
```

`boards.txt`:
```
upload.maximum_size      = 1411072   # 0x158800
upload.maximum_data_size =  229376   # 0x38000
build.extra_flags        = -DSD_BLE_PERIPH_LINK_COUNT=4 -DSD_BLE_CENTRAL_LINK_COUNT=1 -DSD_BLE_HVN_TX_QUEUE_SIZE=3
```

---

## NU54-DK — nRF54L05 + S145 v10.0.1

Source: `bm_nrf54l15dk_nrf54l05_cpuapp_s145_softdevice.dts` in the same repository
Chip capacity: MDK 9.0.2 `nrf54l05_xxaa_application_memory.h`
(`NRF_MEMORY_FLASH_SIZE 0x0007D000`, `NRF_MEMORY_RAM_SIZE 0x00018000`)

### RRAM (500 KB = 0x7D000)

| Start | Size | Region |
|---|---|---|
| `0x00000000` | 354 KB (`0x58800`) | **application (slot0)** |
| `0x00058800` | 4 KB | peer_manager |
| `0x00059800` | 4 KB | storage0 |
| `0x0005A800` | 137 KB (`0x22400`) | **SoftDevice S145** |
| `0x0007D000` | — | End of RRAM |

### RAM (96 KB = 0x18000)

| Start | Size | Region |
|---|---|---|
| `0x20000000` | `0x5D00` | **SoftDevice** |
| `0x20005D00` | `0x12300` (74,496 B) | **application** |
| `0x20018000` | — | End of RAM |

**Measured (NU54-DK hardware, nRF54L05, MTU 247):**

| periph + central · queue | Required app RAM start | Within `0x20005D00` |
|---|---|---|
| 1 + 0 · 1 | `0x20003750` | ✅ |
| 2 + 0 · 1 | `0x200046D8` | ✅ |
| 2 + 0 · 2 | `0x200048D0` | ✅ |
| 2 + 0 · 3 | `0x20004AC8` | ✅ |
| 1 + 1 · 3 | `0x20004AB0` | ✅ central is 24 B cheaper |
| **2 + 1 · 3** | **`0x20005C38`** | ✅ 200 B to spare — **chosen** |
| 3 + 0 · 1 | `0x20005668` | ✅ |

**The numbers are exactly the same as L15 — SoftDevice RAM demand does not depend on the SoC.**
The L05 S145 is a separately relocated build, but its RAM demand follows only the configuration.
So a value measured on one can be used on the other. But when the margin is thin,
**measure instead of carrying it over** — if it misses, that board cannot start BLE at all.

The reservation was grown from `0x4B80` -> `0x5D00` to fit peripheral 2 + central 1.
App RAM went 80,000 -> 74,496 B (5.7 % of 96 KB). This value is for having both multi-peripheral and
central — keeping the old reservation would force giving up one of them
(`2+0` or `1+1`).

> The DTS `app_ram` is `DT_SIZE_K(78)` = `0x13800`, which leaves the top 128 bytes.
> That is rounding down because the device tree writes sizes only in K, not a reserved region.
> The linker script uses everything up to the end of RAM (`0x20018000`).

> **✅ Confirmed on hardware 2026-09-06** (when the SD reservation was `0x4780` / app 80,000 B).
> `__StackTop = 0x20018000`. Flash 36420 B (10% of 362496) / RAM 3856 B (4%).
> millis/micros deltas exact. Log: [HIL/M1-tickless.md](HIL/M1-tickless.md) (Korean)

### Linker script values

```
FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x58800
RAM   (rwx) : ORIGIN = 0x20005D00, LENGTH = 0x12300
```

`boards.txt`:
```
upload.maximum_size      = 362496   # 0x58800
upload.maximum_data_size =  74496   # 0x12300
build.extra_flags        = -DSD_BLE_PERIPH_LINK_COUNT=2 -DSD_BLE_CENTRAL_LINK_COUNT=1 -DSD_BLE_HVN_TX_QUEUE_SIZE=3
```

### The SoftDevice hex is a per-SoC relocated build

They are the same 137 KB, but the load address differs and **they are not interchangeable.**

```
s145_nrf54l05_10.0.1_softdevice.hex : :020000025000AC  -> 0x0005A800
s145_nrf54l15_10.0.1_softdevice.hex : :020000040015E5  -> 0x0015A800
```

`sd.hex` in `platform.txt` picks the file by `{build.sd_soc}`.
If a board's `menu.softdevice.*.build.sd_soc` is wrong, the SoftDevice is written outside RRAM.

---

## XIAO nRF54LM20A — nRF54LM20A + S145 v10.0.1

Source: sdk-nrf-bm v2.0.1
`boards/nordic/bm_nrf54lm20dk/bm_nrf54lm20dk_nrf54lm20a_cpuapp_s145_softdevice.dts`
(+ `..._cpuapp_common.dtsi` in the same directory)

### RRAM (2036 KB = 0x1FD000)

| Start | Size | Region |
|---|---|---|
| `0x00000000` | 1890 KB (`0x1D8800`) | **application (slot0)** |
| `0x001D8800` | 4 KB | peer_manager |
| `0x001D9800` | 4 KB | storage0 |
| `0x001DA800` | 137 KB (`0x22400`) | **SoftDevice S145** |
| `0x001FD000` | — | End of RRAM |

`NRF_MEMORY_FLASH_SIZE 0x001FD000`, the DTS `cpuapp_rram` 2036 K and FICR `INFO.RRAM 0x7F4` agree.

### RAM (**0x7FD40** — not a whole number of KB)

| Start | Size | Region |
|---|---|---|
| `0x20000000` | `0x8000` (32 KB) | **SoftDevice** |
| `0x20008000` | `0x77D40` (490,816 B) | **application** |
| `0x2007FD40` | — | **End of RAM** |

> ⚠ **RAM ends at `0x2007FD40`.** The DTS writes `cpuapp_sram` from `0x20000080` as
> `511K − 0x80 + 0x140` and notes "total size of SRAM is not 1kB aligned".
>
> Reading that as "about 512 KB" and putting the end at `0x20080000` produced **a HardFault on the
> first instruction at boot** (CFSR `0x9201` STKERR, BFAR `0x2007FFF8`). Do not trust other sources:
>
> | Source | Value | |
> |---|---|---|
> | FICR `INFO.RAM` | `0x200` = 512 KB | rounded up to KB |
> | MDK `NRF_MEMORY_RAM_SIZE` | `0x40000` = 256 KB | wrong (the L15 header was wrong too) |
> | MDK `.ld` | RAM `0x40000` + RAM1 `0x40000` | nominal |
>
> Log: [HIL/M6-xiao-nrf54lm20a.md](HIL/M6-xiao-nrf54lm20a.md) §3 (Korean)

**SoftDevice demand measured (XIAO nRF54LM20A, MTU 247): peripheral 4 + central 1 + queue 3 =
`0x20007F48`** — the same as L15·L05. The LM20 S145 also follows only the configuration.
184 B remain in the 32 KB reservation. RAM is plentiful, so to add links, grow the reservation.

The DTS takes `0x20000000 ~ 0x20000080` as a KMU reservation. The core does not use KMU
(CRACEN only for TRNG), and that range is inside the SoftDevice reservation — same as L15.

### Linker script values

```
FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x1D8800
RAM   (rwx) : ORIGIN = 0x20008000, LENGTH = 0x77D40
```

`boards.txt`:
```
upload.maximum_size      = 1935360   # 0x1D8800
upload.maximum_data_size =  490816   # 0x77D40
build.extra_flags        = -DSD_BLE_PERIPH_LINK_COUNT=4 -DSD_BLE_CENTRAL_LINK_COUNT=1 -DSD_BLE_HVN_TX_QUEUE_SIZE=3
```

### SoftDevice hex

`s145_nrf54lm20_10.0.1_softdevice.hex` (389,951 B, sha256 `d1d24495…ffd1`).
Measured load range **`0x001DA800` ~ `0x001FC57C`** (135.4 KB). Not interchangeable with the L15 build.
`build.sd_soc=nrf54lm20` in `boards.txt` picks this file.

---

## For reference — nRF54L15 + S115 v10.0.1 (not adopted)

| Region | Value |
|---|---|
| application | `0x00000000`, 1414 KB (`0x161800`) |
| storage | `0x00161800`, 8 KB |
| SoftDevice | `0x00163800`, 101 KB |
| SD RAM | `0x20000000`, `0x4380` |
| app RAM | `0x20004380`, `0x3BC80` |

S115 is peripheral-only, so the Central family of Bluefruit APIs cannot be used. That is why S145 was fixed
(CLAUDE.md §8). To add it later, only one menu entry in `boards.txt` and one linker script are needed.

---

## ⚠ The opposite of nRF52

| | nRF52 + S140 | **nRF54L15 + S145** |
|---|---|---|
| `0x0` | SoftDevice | **application** |
| Top | application → bootloader | **SoftDevice** |
| Vector table owner | SoftDevice (MBR) | **application** |
| IRQ forwarding | SD forwards to the app | **the app forwards to the SD** |

Do not copy the Adafruit core's `nrf52840_s140_v6.ld` (`FLASH ORIGIN = 0x26000`).
The app starts at `0x0` and owns the vector table, so no VTOR relocation is needed.
Background in CLAUDE.md §7 F1.

---

## M4 (bootloader) layout — the constraints are settled

**Decided** (investigated and measured. CLAUDE.md §7 F11):

- **The bootloader must sit at `0x0`.** The nRF54L has neither an MBR nor `UICR.BOOTLOADERADDR`
  (the MDK has no such symbols), so the CPU boots straight from `0x0`.
  The nRF52 arrangement of "the MBR finds a bootloader at the top" is impossible
- **The application moves up.** The opposite of nRF52
- **The SoftDevice is fixed at `0x0015A800`.** The address is absolute in the hex file
  (measured: `0x0015A800` ~ `0x0017C4F8`, 135.2 KB). It cannot move.
  The address differs by chip — L05 `0x0005A800`, **LM20A `0x001DA800`**
- **Bonding and storage are already outside the app partition** (`0x158800` ~ `0x15A800`).
  A single-bank update does not wipe them. This follows the Nordic DTS
- **The core already handles VTOR relocation.** `init()` in `cores/nrf54l/wiring.c` sets
  `SCB->VTOR` from the linker symbol `__vectors_start`, so it follows the app start address when that
  changes. The MDK startup does not touch VTOR, so without this, interrupts would go to the bootloader
  vectors the moment the app moves

```
0x00000000  Bootloader              <- CPU boots here (size TBD)
0x000?????  Application             <- bootloader jumps here
0x00158800  peer_manager  4 KB      <- outside the app. Bonds survive
0x00159800  storage0      4 KB
0x0015A800  SoftDevice  137 KB      <- fixed
0x0017C4F8  (end)
```

**To decide in M4** (no basis to decide now):

- Bootloader size → app start address. Only building it will tell.
  Building `caveman99/nRF54_Bootloader` first gives a realistic number
- Dual-bank or not. **RRAM has no erase, so swap algorithms behave differently from flash** (R9).
  Do not settle it before hardware verification
- RRAM write block **16 bytes** alignment (R9 / F5). Whether partition boundaries need separate
  alignment is unconfirmed

What changes is one line, `FLASH ORIGIN` in the linker script, and one line,
`upload.maximum_size` in `boards.txt`. Knowing the constraints above matters more than fixing numbers in advance.
