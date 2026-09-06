# M1 실기 검증 — XIAO nRF54L15 Sense

날짜: 2026-09-06
보드: Seeed XIAO nRF54L15 Sense (nRF54L15-CAAA)
프로브: **온보드** ATSAMD11D14A — CMSIS-DAP, VID:PID `2886:0066`, 시리얼 `5784477E`
호스트: macOS / probe-rs 0.32.0 / xPack arm-none-eabi-gcc 14.2.1 / arduino-cli 1.0.3

기존 보드(NU54-DK)에서 잡아 둔 코어가 **다른 보드에서도 그대로 도는지**를 본 것이다.
결과적으로 코어 쪽 결함 두 개가 드러났다 (§3).

---

## 1. 결과 요약

| 항목 | 결과 |
|---|---|
| 실장 칩 판별 (FICR) | ✅ `PART=0x00054B15`, RAM 256 KB, RRAM 1524 KB |
| 온보드 CMSIS-DAP 접속 | ✅ 외부 프로브 불필요 |
| RRAM 쓰기 + verify | ✅ **1.9 초** (38 KB hex) |
| FreeRTOS + GRTC 틱 | ✅ |
| GPIO / LED (P2.00, **active LOW**) | ✅ 육안 확인. 극성이 NU54-DK 와 반대인데 정상 점멸 |
| `Scheduler.startLoop()` 두 번째 태스크 | ✅ |
| `Serial` (UARTE20 → 온보드 SAMD11 → USB CDC) | ✅ |
| 틱 vs SYSCOUNTER | ✅ **0.0 ppm** (180초, 90샘플) |
| 호스트 대비 클럭 | ✅ **-14 ppm** (LFXO 사양 ±20 ppm 안) |
| Δmillis / Δmicros | ✅ 전부 정확히 2000 / 2000000, **이상 0 / 89** |
| 크기 | Flash 38176 B (2%) / RAM 4008 B (1%) |

케이블 하나(USB-C)로 업로드와 시리얼이 모두 됐다.
LED 점멸과 `Serial` 출력을 **릴리스 0.1.0 설치본으로도 다시 확인**했다
(Board Manager 로 설치한 코어 → 컴파일 → 업로드 → 동작).

---

## 2. FICR 판독

```
0x00FFC31C: 00054b15 41414230 00004341 00000100 000005f4
             PART     VARIANT  PACKAGE  RAM      RRAM
             nRF54L15 "AAB0"   "CA"     256 KB   1524 KB
0x00FFC624: 013d0015   XOSC32KTRIM  SLOPE=21 OFFSET=317
```

이 과정에서 **`docs/MEMORY-MAP.md` 의 FICR 오프셋이 4바이트씩 틀려 있던 것**을
찾아 고쳤다. RAM 은 `0x00FFC328`, RRAM 은 `0x00FFC32C` 이고 마지막 필드 이름도
`CODESIZE` 가 아니라 `RRAM` 이다. 근거는 MDK `NRF_FICR_INFO_Type`.

---

## 3. 잡은 결함 — 둘 다 코어에 보드 사실이 박혀 있었다

### (1) `Serial` 이 죽는다 — UARTE 벡터가 하드코딩돼 있었다

`Uart.cpp` 가 `SERIAL30_IRQHandler` 를 직접 정의하고 있었다. NU54-DK 는 UARTE30 을
쓰니 맞지만, 이 보드는 **UARTE20** 이라 벡터가 연결되지 않는다.
CLAUDE.md §7 F10 ③ 이 경고한 그 함정이 그대로 재현될 상황이었다
(`Serial.println()` 이 `flush()` 에서 영원히 대기).

→ 벡터 이름을 **variant 가 준다** (`SERIAL_UARTE_IRQ_HANDLER`).
  확인:

```
XIAO      000014d4 T SERIAL20_IRQHandler   /  ... W SERIAL30_IRQHandler
NU54-DK   ... W SERIAL20_IRQHandler        /  000014d8 T SERIAL30_IRQHandler
```

### (2) 클럭이 +805 ppm — LFXO 로드 커패시터가 하드코딩돼 있었다 ⭐

`port_grtc.c` 가 `NRF_OSCILLATORS_LFXO_CAP_EXTERNAL`(=0)을 써 넣고 있었다.
주석에도 "외부 캡이 실장돼 있으므로" 라고 적혀 있었는데, 그건 **NU54-DK 의 보드
사실이지 칩의 성질이 아니다.** 이 보드에는 외부 캡이 없고 내부 캡을 쓴다.

| 설정 | 호스트 대비 |
|---|---|
| INTCAP = 0 (외부 캡) | **+805 ppm** |
| 내부 캡 16000 fF | **-14 ppm** |

**증상이 없다는 게 이 버그의 성질이다.** 타깃 안에서는 완벽하다 —
틱 vs SYSCOUNTER 0 ppm, Δmillis 전부 2000, Δmicros 전부 2000000.
호스트 시계와 비교해야만 드러난다. §7 F12 와 같은 계열이다.
그리고 +805 ppm 은 BLE 요구치 ±250 ppm 을 넘으므로 **M3 에서 연결이 끊긴다.**

→ variant 가 용량(fF)을 주고 코어가 `INTCAP` 을 계산한다.

> **상수를 박으면 안 된다.** 변환에 `FICR.XOSC32KTRIM` 이 들어가고 그 트림은
> **칩 개체마다 다르다.** 이 개체는 SLOPE 21 / OFFSET 317 → `INTCAP = 21` 이지만
> 다른 개체는 다른 값이 나온다. 그래서 런타임 계산이다.
>
> nrfx 의 `NRF_OSCILLATORS_LFXO_CAP_CALCULATE` 는 쓰지 마라.
> `((SLOPE + 392) >> 9) * (cap*2-12)` 라 앞항이 0 으로 잘려서
> **cap 값과 무관하게 같은 값이 나온다** (이 칩에서 6/7/9/11 pF 전부 4).
> Zephyr `soc/nordic/nrf54l/soc.c` 가 PS 공식을 정수로 제대로 구현해 놨다.

---

## 4. 값의 출처 — 벤더 보드 정의를 먼저 봐라

회로도만으로는 두 가지를 알 수 없었다: LFXO 로드 용량, D6/D7 의 UART 인스턴스.
upstream Zephyr 에 이 보드가 이미 있다 (`boards/seeed/xiao_nrf54l15/`).

| 항목 | Zephyr | 회로도에서 읽은 것 |
|---|---|---|
| `uart20` TX/RX | P1.09 / P1.08 | 일치 |
| LED | `gpio2 0 GPIO_ACTIVE_LOW` | 일치 |
| 버튼 | `gpio0 0 PULL_UP, ACTIVE_LOW` | 일치 |
| `i2c22` / `i2c30` | P1.10·P1.11 / P0.04·P0.03 | 일치 |
| `spi00` | P2.01·P2.02·P2.04 | 일치 |
| `pdm20` | P1.12 / P1.13 | 일치 |
| **LFXO** | **internal, 16000 fF** | **알 수 없었음** (크리스털 각인은 7 pF) |
| **D6/D7** | **uart21, P2.08 / P2.07** | "UART21" 표기만 있었음 |

**교훈: 새 보드를 붙일 때 벤더 Zephyr 보드 정의를 먼저 찾아봐라.**
회로도로는 알 수 없는 값이 거기 들어 있다. 크리스털 부품 각인(7 pF)을 믿고
갔으면 틀렸을 것이다 — 정답은 16 pF 였다.

---

## 5. 남은 것

- **`Serial1`(D6/D7, UARTE21) 미배정.** Zephyr 은 붙이는데, 그러면 인스턴스 21 이
  P2 를 쓰는 셈이라 `docs/PERIPHERAL-PINMAP.md` 의 도메인 규칙과 어긋난다.
  규칙을 Product Specification 으로 바로잡은 뒤 붙인다.
  검증은 D6-D7 을 점퍼로 잇고 루프백하면 된다 (M2)
- **RF 스위치(P2.03/P2.05)의 실제 동작 미확인.** `initVariant()` 가 전원을 켜고
  RF1(온보드 안테나)로 잡아 두지만, RF1 이 정말 온보드인지는 M3 에서 확인한다
- **전류 미측정.** 이 보드는 벅 컨버터라 NU54-DK(LDO)와 조건이 다르다.
  RF 스위치 전원 몫도 빼야 한다
- **온보드 IMU / 마이크 미사용.** M2 이후

---

## 6. 재현 방법

```sh
# 보드는 USB-C 하나만 연결한다 (외부 프로브 불필요)
arduino-cli compile --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 --build-path /tmp/b <스케치>
arduino-cli upload  -b baram-nrf54:nrf54l:xiao_nrf54l15 --input-dir /tmp/b <스케치>

# 벡터가 붙었는지 (W 면 죽는다)
arm-none-eabi-nm /tmp/b/*.elf | grep SERIAL20_IRQHandler   # -> T

# 클럭 정확도: readline() 으로 받고 즉시 타임스탬프를 찍는다.
# read(n) 은 타임아웃까지 블록해서 도착 시각이 양자화된다 (§7 F12).
```

⚠ **업로드 직후에는 USB CDC 포트가 잠깐 사라진다.** probe-rs 가 리셋을 걸면
SAMD11 쪽 포트 핸들이 무효가 되어 `[Errno 6] Device not configured` 가 난다.
포트를 다시 열 때까지 재시도하는 루프를 넣어라.
