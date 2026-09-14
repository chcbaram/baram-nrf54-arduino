# License inventory

*[English](LICENSE-INVENTORY.md) · [한국어](LICENSE-INVENTORY.ko.md)*

The document for CLAUDE.md §9 / R3 / R4. **The licensing is mixed**, and it is not "open source" by the OSI definition.

> Status: **first survey done / per-file scan not done.**
> After the `nrf54l/` vendoring is finished, run `reuse lint` or `scancode-toolkit` to fill in §4.

---

## 1. Conclusion — the SoftDevice binary can be redistributed

This was a candidate STOP condition for M0. **The survey found no problem.**

`components/softdevice/nrf54l/s145/s145_10.0.1_license-agreement.txt` in sdk-nrf-bm v2.0.1 is
**the same full `LicenseRef-Nordic-5-Clause` text** as the repository root `LICENSE`, and ends with:

```
SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
NCS-SBOM-Apply-To-File: ./*.hex
```

So the 5-Clause applies to the hex files as is. **Unlike S110/S130 in the nRF5 days, no separate restrictive
SoftDevice License Agreement is attached.** Clause 2 explicitly allows binary redistribution.

→ The hex is bundled in `nrf54l/softdevice/`. The download approach (alternative A) is unnecessary.

---

## 2. Nordic-5-Clause, clause by clause

| Clause | Content | Our response |
|---|---|---|
| 1 | Keep notices, conditions and disclaimer when redistributing source | Keep notices at the top of vendored files (CLAUDE.md §12) |
| 2 | **Binary redistribution allowed.** The notice must be reproduced in accompanying documentation | `softdevice/LICENSE-Nordic` + stated in the README |
| 3 | No promotion using the Nordic name | "Nordic" not used in the core name or description. `BARAM` / `NU54-DK` |
| 4 | May be used only with Nordic ICs | Stated in the README. **This is why it is not OSI open source** |
| 5 | No reverse engineering, modification or disassembly of the binary | R3. The hex is bundled unmodified |

**Clauses 4·5 conflict with GPL's "no further restrictions"**, so our own code is MIT (R4).

Related item: `s145_10.0.1_license-attribution.txt` carries an **ARM BSD-3-Clause** notice.
It means the SoftDevice contains ARM code, so that file is bundled too, as `nrf54l/softdevice/LICENSE-attribution`.

---

## 3. License by component (plan)

| Component | Source | License | Distributed at |
|---|---|---|---|
| The core's own code | Newly written | **MIT** | `nrf54l/cores/nrf54l/` |
| SoftDevice S145 hex | sdk-nrf-bm v2.0.1 | **LicenseRef-Nordic-5-Clause** (+ ARM BSD-3-Clause) | `nrf54l/softdevice/` |
| SoftDevice API headers | sdk-nrf-bm v2.0.1 | LicenseRef-Nordic-5-Clause | `nrf54l/cores/nrf54l/nordic/softdevice/` |
| `softdevice_handler` port | sdk-nrf-bm v2.0.1 | LicenseRef-Nordic-5-Clause | `nrf54l/cores/nrf54l/nordic/` |
| nrfx 4.x | NordicSemiconductor/nrfx | BSD-3-Clause | `nrf54l/cores/nrf54l/nordic/nrfx/` |
| MDK (`nrf54l15.h` etc.) | nrfx `mdk/` | BSD-3-Clause | `nrf54l/cores/nrf54l/nordic/nrfx/mdk/` |
| CMSIS-Core (M33) | ARM-software/CMSIS_5 or _6 | Apache-2.0 | `nrf54l/cores/nrf54l/nordic/cmsis/` |
| FreeRTOS-Kernel | FreeRTOS/FreeRTOS-Kernel | **MIT** | `nrf54l/cores/nrf54l/freertos/` |
| Arduino API family (`Print`/`Stream`/`WString` etc.) | Arduino / Adafruit nRF52 core lineage | **LGPL-2.1** ⚠ | `nrf54l/cores/nrf54l/` |
| Bluefruit52Lib port (M3) | adafruit/Adafruit_nRF52_Arduino | BSD-3-Clause / MIT (varies by file) | `nrf54l/libraries/Bluefruit54Lib/` |
| micro-ecc (P-256 for LESC) | kmackay/micro-ecc | **BSD-2-Clause** | `nrf54l/libraries/Bluefruit54Lib/src/utility/micro-ecc/` |
| probe-rs binary | probe-rs/probe-rs | MIT OR Apache-2.0 | **Not in the repository.** Distributed only as an asset of the `probe-rs-0.32.0` release (repackaged) |

> ✅ **Confirmed (at porting time)**: the headers of the ported Arduino core API files were checked directly.
> `Print.cpp` / `Stream.cpp` / `WString.cpp` etc. are indeed **LGPL-2.1**
> ("modify it under the terms of the GNU Lesser General Public").
> What R4 forbids is **GPL**; LGPL brings separate obligations when statically linked.
> Existing Arduino cores including Adafruit and stm32duino all ship in the same state,
> so there is ample precedent. Newly written files are MIT.
>
> Ported LGPL files: `Print` `Stream` `WString` `WMath` `WCharacter` `RingBuffer`
> `HardwareSerial.h` `Printable.h` `IPAddress` `Client.h` `Server.h` `Udp.h`
> `itoa` `hooks.c` `wiring_shift` `new.cpp` `abi.cpp` `binary.h` `avr/*`
> (the original notices are kept at the top of each file.)

---

## 4. Per-file scan results

*(to be filled in after vendoring)*

```
# planned
pipx run reuse lint
# or
pipx run scancode-toolkit --license --json-pp docs/scancode.json nrf54l/
```

| Path | License | SPDX header | Notes |
|---|---|---|---|
| *(not written)* | | | |

---

## 5. What the README must say (CLAUDE.md §9)

- The core's own code is **MIT**
- The bundled SoftDevice is **Nordic-5-Clause**, **Nordic ICs only**
- **Do not flatly call it "open source"** — clauses 4·5 mean it does not meet the OSI definition. Describe it as **"mixed licensing"**
- Describe support scope in three levels: `supported` / `partially supported` / `not supported` (CLAUDE.md §11)

### Precedent

`adafruit/Adafruit_nRF52_Arduino` and the Seeed·smartme.io·CAMI forks bundle the SoftDevice and distribute
through the Board Manager, with no reported problems.

## What was changed when porting — micro-ecc

A curve selection was added before the default configuration block of `uECC.h`. The original enables all curves,
but we use **only P-256 (secp256r1)**, and the rest only take flash.
LICENSE.txt is included alongside, and the code itself was not modified.

⚠ `uECC_VLI_NATIVE_LITTLE_ENDIAN` was **not touched.** The Nordic documentation warns that changing it can
break things. BLE exchanges keys little-endian, so the endian conversion is done explicitly in `BLESecurity.cpp`.
