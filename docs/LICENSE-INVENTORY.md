# 라이선스 인벤토리

CLAUDE.md §9 / R3 / R4에 대응하는 문서. **혼재 라이선스**이며 OSI 정의의 "오픈소스"가 아니다.

> 상태: **1차 조사 완료 / 파일 단위 스캔 미완.**
> `nrf54l/` vendoring이 끝난 뒤 `reuse lint` 또는 `scancode-toolkit`을 돌려 §4를 채운다.

---

## 1. 결론 — SoftDevice 바이너리 재배포 가능

M0의 STOP 조건 후보였던 항목이다. **조사 결과 문제없다.**

sdk-nrf-bm v2.0.1의 `components/softdevice/nrf54l/s145/s145_10.0.1_license-agreement.txt`는
저장소 루트 `LICENSE`와 **동일한 `LicenseRef-Nordic-5-Clause` 전문**이며, 말미에 이렇게 명시한다:

```
SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
NCS-SBOM-Apply-To-File: ./*.hex
```

즉 hex 파일에 5-Clause가 그대로 적용된다. **nRF5 시절 S110/S130처럼 별도의 제한적
SoftDevice License Agreement가 붙어 있지 않다.** 2항이 바이너리 재배포를 명시적으로 허용한다.

→ `nrf54l/softdevice/`에 hex를 번들한다. 다운로드 방식(대안 A)은 불필요.

---

## 2. Nordic-5-Clause 조항별 대응

| 조항 | 내용 | 우리 대응 |
|---|---|---|
| 1 | 소스 재배포 시 고지·조건·면책 유지 | vendoring 파일 상단 고지 유지 (CLAUDE.md §12) |
| 2 | **바이너리 재배포 허용.** 동봉 문서에 고지 재현 필요 | `softdevice/LICENSE-Nordic` + README에 명시 |
| 3 | Nordic 이름으로 홍보 금지 | 코어 이름·설명에 "Nordic" 미사용. `BARAM` / `NU54-DK` |
| 4 | Nordic IC에서만 사용 가능 | README에 명시. **이것 때문에 OSI 오픈소스가 아님** |
| 5 | 바이너리 리버스엔지니어링·수정·디스어셈블 금지 | R3. hex 무수정 번들 |

**4·5항이 GPL의 "추가 제약 금지"와 충돌**하므로 자체 코드는 MIT로 간다 (R4).

부수 항목: `s145_10.0.1_license-attribution.txt`에 **ARM BSD-3-Clause** 고지가 들어 있다.
SoftDevice가 ARM 코드를 포함한다는 뜻이므로 이 파일도 `nrf54l/softdevice/LICENSE-attribution`으로 함께 동봉한다.

---

## 3. 구성 요소별 라이선스 (계획)

| 구성 요소 | 출처 | 라이선스 | 배포 위치 |
|---|---|---|---|
| 코어 자체 코드 | 신규 작성 | **MIT** | `nrf54l/cores/nrf54l/` |
| SoftDevice S145 hex | sdk-nrf-bm v2.0.1 | **LicenseRef-Nordic-5-Clause** (+ ARM BSD-3-Clause) | `nrf54l/softdevice/` |
| SoftDevice API 헤더 | sdk-nrf-bm v2.0.1 | LicenseRef-Nordic-5-Clause | `nrf54l/cores/nrf54l/nordic/softdevice/` |
| `softdevice_handler` 이식본 | sdk-nrf-bm v2.0.1 | LicenseRef-Nordic-5-Clause | `nrf54l/cores/nrf54l/nordic/` |
| nrfx 4.x | NordicSemiconductor/nrfx | BSD-3-Clause | `nrf54l/cores/nrf54l/nordic/nrfx/` |
| MDK (`nrf54l15.h` 등) | nrfx `mdk/` | BSD-3-Clause | `nrf54l/cores/nrf54l/nordic/nrfx/mdk/` |
| CMSIS-Core (M33) | ARM-software/CMSIS_5 or _6 | Apache-2.0 | `nrf54l/cores/nrf54l/nordic/cmsis/` |
| FreeRTOS-Kernel | FreeRTOS/FreeRTOS-Kernel | **MIT** | `nrf54l/cores/nrf54l/freertos/` |
| Arduino API 계열 (`Print`/`Stream`/`WString` 등) | Arduino / Adafruit nRF52 코어 계보 | **LGPL-2.1** ⚠ | `nrf54l/cores/nrf54l/` |
| Bluefruit52Lib 이식본 (M3) | adafruit/Adafruit_nRF52_Arduino | BSD-3-Clause / MIT (파일별 상이) | `nrf54l/libraries/Bluefruit54Lib/` |
| probe-rs 바이너리 | probe-rs/probe-rs | MIT OR Apache-2.0 | `nrf54l/tools/probe-rs/` |

> ✅ **확인 완료 (이식 시점)**: 이식한 Arduino 코어 API 파일들의 헤더를 직접 확인했다.
> `Print.cpp` / `Stream.cpp` / `WString.cpp` 등은 **LGPL-2.1** 이 맞다
> ("modify it under the terms of the GNU Lesser General Public").
> R4가 금지하는 것은 **GPL** 이며 LGPL은 정적 링크 시 별도 의무가 생긴다.
> Adafruit·stm32duino를 포함한 기존 Arduino 코어들이 모두 같은 상태로 배포 중이므로
> 전례는 충분하다. 새로 쓰는 파일은 MIT로 간다.
>
> 이식한 LGPL 파일 목록: `Print` `Stream` `WString` `WMath` `WCharacter` `RingBuffer`
> `HardwareSerial.h` `Printable.h` `IPAddress` `Client.h` `Server.h` `Udp.h`
> `itoa` `hooks.c` `wiring_shift` `new.cpp` `abi.cpp` `binary.h` `avr/*`
> (원본 고지는 각 파일 상단에 그대로 유지했다.)

---

## 4. 파일 단위 스캔 결과

*(vendoring 후 채운다)*

```
# 실행 예정
pipx run reuse lint
# 또는
pipx run scancode-toolkit --license --json-pp docs/scancode.json nrf54l/
```

| 경로 | 라이선스 | SPDX 헤더 유무 | 비고 |
|---|---|---|---|
| *(미작성)* | | | |

---

## 5. README에 반드시 쓸 것 (CLAUDE.md §9)

- 코어 자체 코드는 **MIT**
- 번들된 SoftDevice는 **Nordic-5-Clause**, **Nordic IC 전용**
- **"오픈소스"라고 단정하지 말 것** — 4·5항 때문에 OSI 정의를 충족하지 않는다. **"혼재 라이선스"** 로 표기
- 지원 범위는 `지원` / `부분 지원` / `미지원` 3단계로 (CLAUDE.md §11)

### 전례

`adafruit/Adafruit_nRF52_Arduino`와 Seeed·smartme.io·CAMI 포크들이 SoftDevice를 번들해
Board Manager로 배포 중이며 문제된 사례가 없다.
