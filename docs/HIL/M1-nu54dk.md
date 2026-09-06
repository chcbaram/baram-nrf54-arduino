# M1 실기 검증 — NU54-DK

날짜: 2026-09-05
보드: NU54-DK (모듈 NCRB54N01VC)

> ⚠ **이 문서의 측정은 보드를 nRF54L15 로 알고 진행했다. 실제 실장 칩은 nRF54L05 다.**
> 나중에 FICR 로 확인해 드러났고, 그때까지 L15 링커 스크립트로 빌드해 돌고 있었다.
> 같은 다이의 비닝이라 사양 밖 RRAM/RAM 이 물리적으로 있어서 **아무 증상도 없었다**
> ([MEMORY-MAP.md](../MEMORY-MAP.md) 의 경고가 이 사례다).
> 아래의 `flash 33872 / 1411072` 같은 용량 수치는 **L15 기준**이라 현재 값과 다르다.
> 핀맵·레지스터·타이밍 측정은 두 칩이 동일하므로 그대로 유효하다.
프로브: **NU-DAP** — CMSIS-DAP, VID:PID `0d28:0204` (Arm), J3(10핀 1.27mm) 연결
호스트: macOS / probe-rs 0.32.0 / arm-none-eabi-gcc 14.2.1

---

## 1. 결과 요약

| 항목 | 결과 |
|---|---|
| probe-rs SWD 접속 | ✅ |
| RRAM 쓰기 + verify | ✅ 3.3초 (33KB hex) |
| 부팅 → `init()` → `initVariant()` | ✅ GPIO 레지스터로 확인 |
| FreeRTOS 스케줄러 기동 | ✅ |
| GRTC 틱 | ✅ ~1000 Hz |
| `loop()` 태스크 | ✅ LED D7 점멸 (육안) |
| `Scheduler.startLoop(loop2)` 두 번째 태스크 | ✅ LED D8 점멸 (육안 + 레지스터) |
| `Serial` (UARTE30 → CP2102N) | ✅ |
| `millis()` / `micros()` 정확도 | ✅ 아래 참조 |
| 버튼 내부 풀업 | ✅ |
| 10분 연속 무크래시 | 진행 중 |
| tickless idle | 미착수 (단계 4) |
| Arduino IDE 업로드 recipe | **미검증** — probe-rs 직접 호출로만 확인 |

## 2. 측정값

### 시간 정확도

```
millis=2002 micros=2002016 btn1=1
millis=4002 micros=4002016 btn1=1
millis=6002 micros=6002016 btn1=1
```

스케치는 `delay(1000)` 을 두 번 돈다.

- `millis` 델타 = **정확히 2000**
- `micros` 델타 = **정확히 2000000**
- `micros - millis*1000` = **+16 µs 고정** (틱 기준점과 첫 틱 사이의 상수 오프셋. 누적되지 않음)

### 크기

```
text 33040  data 832  bss 2948
flash 33872 / 1411072  (2.4%)
정적 RAM 3780 / 243328 (1.6%)
힙 가용 232 KB (__HeapBase 0x200055C4 ~ __StackLimit 0x2003F800)
```

### 메모리 배치 (실측, `docs/MEMORY-MAP.md` 와 일치)

```
.vectors  @ 0x00000000
.text     @ 0x00000E08
.data     @ 0x20004780   ← SoftDevice RAM 바로 위
.bss      @ 0x20004A80
__StackTop  0x20040000
__StackLimit 0x2003F800
```

### GPIO 레지스터 확인

`initVariant()` 직후 SWD 로 읽은 값:

| 레지스터 | 값 | 해석 |
|---|---|---|
| P1 DIR | `0x00004400` | bit10(LED D8) + bit14(LED D10) 출력 |
| P2 DIR | `0x00000280` | bit7(LED D9) + bit9(LED D7) 출력 |
| P1 IN | `0x00002300` | SW4/SW3/SW2 HIGH = 내부 풀업 동작 |
| P1/P2 OUT | `0x0` | LED 전부 off (active HIGH) |

UARTE30 PSEL 확인 (`0x50104604`, 순서는 **TXD, CTS, RXD, RTS**):

```
0, 2, 1, 3  →  TXD=P0.00  CTS=P0.02  RXD=P0.01  RTS=P0.03   모두 정확
```

---

## 3. nRF52 와 다른 레지스터 오프셋 (주의)

nRF52 코드를 옮길 때 그대로 쓰면 엉뚱한 주소를 읽는다.

| | nRF52 | **nRF54L15** |
|---|---|---|
| GPIO `OUT` | 베이스 + `0x504` | 베이스 + **`0x000`** |
| GPIO `IN` | + `0x510` | + **`0x00C`** |
| GPIO `DIR` | + `0x514` | + **`0x010`** |
| 리셋 원인 | `NRF_POWER->RESETREAS` | **`NRF_RESET`** (`NRF_RESETINFO` 도 아니다) |

베이스 주소: `P0 0x5010A000`, `P1 0x500D8200`, `P2 0x50050400`, `UARTE30 0x50104000`, `GRTC 0x500E2000`

---

## 4. 잡은 버그 (전부 호스트 컴파일로는 안 드러남)

상세는 커밋 `10efd2f` 와 `CLAUDE.md` §7 F1 / F3 / F10 참조.

1. **FreeRTOS SVC 번호가 SoftDevice 영역과 충돌** — 11.3.1 이 100~105 를 쓴다. 0~5 로 옮김.
   증상: `svc 102` → SoftDevice 분기 → 벡터[2] = `NMI_Handler` 무한루프. **폴트도 안 난다**
2. **nrfx GRTC 채널 마스크/개수 불일치** → `nrfx_grtc_init()` 이 `-ECANCELED`
3. **`NRFX_UARTE_INSTANCE(30)`** — nrfx 4.x 는 포인터를 받는다. `p_reg=30` → BusFault `BFAR 0x21E`
4. **UARTE 벡터 미연결** — nrfx 가 다중 인스턴스 핸들러를 안 만든다. `Serial.println()` 이 `flush()` 에서 정지
5. **하드웨어 흐름제어 기본 on** — 호스트가 RTS 를 안 올려 TX 영구 대기. 흐름제어 제거
6. **`micros()` 가 부팅 시 1.47e9** — SYSCOUNTER 는 리셋으로 0 이 되지 않는다. 기준점 차감

---

## 5. 디버깅 방법 메모 (다음에 또 쓴다)

### probe-rs 로 타깃 RAM 읽기

```
probe-rs read --chip nRF54L15 --non-interactive b32 <주소> <워드수>
```

전역 변수 주소는 `arm-none-eabi-nm blink.elf | grep <심볼>` 로 얻는다.

### ⚠ probe-rs 호출마다 attach/detach 하며 **코어가 재개된다**

그래서 `DHCSR` 로 halt 시킨 뒤 다음 호출에서 레지스터를 읽는 방식은 **믿을 수 없다**
(`S_HALT` 가 0 으로 돌아오고 `DCRDR` 에 직전 값이 남는다).

→ **펌웨어가 스스로 정보를 남기게 하는 편이 확실하다.** 그래서 두 개를 넣었다:

- `cores/nrf54l/fault_handler.c` 의 `g_fault` — 예외 프레임 + CFSR/HFSR/BFAR/EXC_RETURN.
  `magic == 0xFA0175ED` 이면 폴트가 잡힌 것이다
- `rtos.cpp` 의 `g_assert_file` / `g_assert_line` — `configASSERT` / `NRFX_ASSERT` 실패 위치

`EXC_RETURN` 이 특히 유용했다. `0xFFFFFFFD` 면 Thread/PSP → **태스크 안에서** 터진 것이라
스케줄러가 이미 돌고 있었다는 뜻이다.

### zsh 주의

zsh 는 인용하지 않은 변수를 단어 분할하지 않는다.
`CHIP="--chip X --non-interactive"; probe-rs read $CHIP ...` 는 통째로 한 인자가 되어 실패한다.
배열을 써라: `CHIP=(--chip X --non-interactive)` + `"${CHIP[@]}"`.
