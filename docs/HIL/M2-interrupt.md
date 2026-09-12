# M2 실기 검증 — `attachInterrupt` (GPIOTE)

날짜: 2026-09-12
보드: NU54-DK (nRF54L05, variant `nu54dk`)
프로브: **NU-DAP** — CMSIS-DAP `0d28:0204`, J3 연결
호스트: macOS / probe-rs 0.32.0 / arm-none-eabi-gcc 14.2.1

이 보드를 고른 이유는 **버튼이 두 도메인에 걸쳐 있기 때문**이다. GPIOTE 는 둘뿐이고
각자 자기 포트만 보므로, 한 도메인만 있는 보드에서는 절반밖에 검증되지 않는다.

---

## 1. 결과 요약

| 항목 | 결과 |
|---|---|
| GPIOTE20 (P1) 인터럽트 | ✅ SW2·SW3·SW4 |
| GPIOTE30 (P0) 인터럽트 | ✅ SW5 |
| `FALLING` / `CHANGE` 모드 구분 | ✅ 아래 §2 |
| P2 핀 거부 (`attachInterruptOk` → false) | ✅ 음성 시험 |
| 벡터 4개 `T` 로 링크 | ✅ §3 |
| 안 쓰는 스케치의 크기 증가 | ✅ **+88 B** (처음엔 0 B 로 **잘못 쟀다**) §4 |

---

## 2. 버튼 시험

```
SW2  P1.13  FALLING   → 1    (1회 누름)
SW3  P1.09  FALLING   → 2    (2회 누름)
SW4  P1.08  CHANGE    → 6    (3회 누름 — 누름+뗌 각각)
SW5  P0.04  FALLING   → 2    (2회 누름)
```

**SW4 가 6 으로 찍힌 것이 핵심 방증이다.** `CHANGE` 만 다른 셋의 2배로 세므로,
모드가 실제로 트리거 설정까지 내려가고 있다는 뜻이다. 카운터를 그냥 올리는
구현이라면 넷이 같은 배율로 나왔을 것이다.

부팅 로그:

```
attachInterrupt test
SW2 P1.13 ok
SW3 P1.09 ok
SW4 P1.08 ok
SW5 P0.04 ok
P2.09 (없어야 함) 거부됨  <-- 맞다
```

### 음성 시험 — P2

`attachInterruptOk(_PINNUM(2, 9), ...)` 가 `false` 를 돌려준다.
**P2 를 담당하는 GPIOTE 가 아예 없다** (Pin Planner 의 SoC 정의로 확인,
[PERIPHERAL-PINMAP.md](../PERIPHERAL-PINMAP.md) §4). 조용히 아무 일도 안 하는 대신
호출자가 알 수 있게 한 이유가 이것이다 — Arduino 표준 `attachInterrupt()` 는
반환값이 없어서 실패를 알릴 방법이 없다.

---

## 3. 벡터 연결 확인 (§7 F10 ③)

GPIOTE 는 **다중 인스턴스**라 nrfx 가 벡터 심볼을 만들어 주지 않는다. 직접 이었다.

```
$ arm-none-eabi-nm blink_base.ino.elf | grep -i gpiote
00005764 T GPIOTE20_0_IRQHandler
00005770 T GPIOTE20_1_IRQHandler
00005774 T GPIOTE30_0_IRQHandler
00005780 T GPIOTE30_1_IRQHandler
00003030 T nrfx_gpiote_irq_handler
```

넷 다 `T`. **인스턴스마다 벡터가 둘(`_0` / `_1`)이라는 점을 놓치기 쉽다** —
하나만 이으면 그쪽 이벤트만 처리되고 나머지는 조용히 사라진다.

---

## 4. 크기 — 처음 잰 값이 틀렸다

> ⚠ **이 절은 2026-09-12 에 통째로 고쳤다.** 처음에는 "증가 0 B" 라고 적었고
> 커밋 메시지에도 그렇게 썼는데, **기준선을 잘못 잡은 것이었다.**

`attachInterrupt` 는 스케치가 `#include` 없이 부르므로 **코어에 있어야 한다.**
그런데 `core.a` 는 `-Wl,--whole-archive` 로 링크되므로(§7 F13 ①) 안 쓰는
스케치까지 무거워질 수 있다 — `Wire` 를 코어에서 라이브러리로 옮긴 이유가 그것이었다.

### 오측정의 원인

`cores/` 아래 소스는 **무조건 컴파일된다**(§7 F13 ②). `wiring_interrupt.c` 를
만들어 둔 뒤에 "붙이기 전" 을 쟀으니, 그 기준선에 이미 GPIOTE 가 들어 있었다.
그 뒤 `nrfx_config.h` 와 `Arduino.h` 만 고쳤는데 둘 다 링크에 영향이 없어
**앞뒤가 똑같이 25,264 B 로 나왔다.**

→ **파일을 실제로 치우고 다시 재야 한다.** 그렇게 얻은 진짜 기준선은 **23,696 B** 다.

### 실측 (blink: `pinMode` + `digitalToggle` + `Serial`)

| | flash | 증가 |
|---|---|---|
| 기준선 (GPIOTE·PWM 없음) | 23,696 B | — |
| 벡터가 `nrfx_gpiote_irq_handler` 를 직접 호출 | 25,264 B | **+1,568** |
| 거기에 PWM 까지 직접 호출 | 25,840 B | **+2,144** |
| **함수 포인터 트램폴린 (현재)** | **23,784 B** | **+88** |

### 왜 직접 부르면 안 되나

**벡터 테이블은 링커 스크립트가 `KEEP` 한다.** 그래서 벡터가 드라이버 핸들러를
직접 부르면 `--gc-sections` 이 그 드라이버를 **절대** 못 걷어낸다. 스케치가
`attachInterrupt` 를 한 번도 안 써도 `nrfx_gpiote_irq_handler`(484 B)와
인스턴스 데이터(256 B)와 nrfy 헬퍼가 전부 남는다.

→ 포인터를 한 겹 둔다. 벡터는 트램폴린만 붙잡고, 진짜 핸들러는 그것을
**설정하는 코드**(`ensure_init()`) 에서만 참조된다. 안 쓰면 통째로 사라진다.

```c
static irq_vector_fn_t s_isr20;
static void dispatch20(void) { nrfx_gpiote_irq_handler(&m_gpiote20); }
void GPIOTE20_0_IRQHandler(void) { if (s_isr20) s_isr20(); }
/* ensure_init() 안에서:  s_isr20 = dispatch20; */
```

확인:

```
$ nm blink.elf | grep -E "GPIOTE.*IRQHandler|nrfx_gpiote_irq_handler"
00005294 T GPIOTE20_0_IRQHandler      <- 벡터는 살아 있다
...                                   <- nrfx_gpiote_irq_handler 는 없다
```

### 앞서 적었던 일반화도 틀렸다

처음에 *"코어에 전역 객체를 두면 모든 스케치가 비용을 치르고, 함수만 두면 부르는
스케치만 치른다"* 고 적었다. 전반부는 맞지만 **후반부가 틀렸다** —
벡터 테이블이 참조하면 함수도 살아남는다.

> **맞는 규칙**: `--gc-sections` 의 루트는 **`KEEP` 된 섹션**이다. 우리 경우
> 벡터 테이블과 `.init_array` 다. 거기서 도달할 수 있는 것은 무엇이든 모든
> 스케치에 들어간다. **코어에 페리페럴을 붙일 때는 벡터에서 드라이버까지의
> 경로를 반드시 끊어라.**

## 5. 재현

```sh
arduino-cli upload -b "baram-nrf54:nrf54l:nu54dk:upload_method=cmsisdapuid" \
  --upload-field probe_id=0d28:0204:<serial> <sketch>
```

⚠ 프로브가 두 대 이상 꽂혀 있으면 기본 `cmsisdap` 항목이
`Error: Failed to parse probe index` 로 죽는다. probe-rs 가 대화형으로 고르려 하는데
arduino-cli 가 stdin 을 주지 않기 때문이다. **UID 항목을 쓰라** (§3 에서 이 메뉴를
처음부터 넣어 둔 이유가 이것이다).

시리얼은 CP2102N 에 DTR 이 연결돼 있지 않아(§4) **포트를 열어도 리셋되지 않는다.**
부팅 로그를 보려면 포트를 먼저 열고 `probe-rs reset` 을 따로 쳐야 한다.
