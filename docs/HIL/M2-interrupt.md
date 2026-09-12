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
| 안 쓰는 스케치의 크기 증가 | ✅ **0 B** §4 |

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

## 4. 크기 — 코어에 넣어도 공짜다

`attachInterrupt` 는 스케치가 `#include` 없이 부르므로 **코어에 있어야 한다.**
그런데 `core.a` 는 `-Wl,--whole-archive` 로 링크되므로(§7 F13 ①) 안 쓰는 스케치까지
무거워질 수 있다 — `Wire` 를 코어에서 라이브러리로 옮긴 이유가 그것이었다.

실측했다. blink (`pinMode` + `digitalToggle` + `Serial`):

| | flash |
|---|---|
| GPIOTE 붙이기 전 | 25,264 B |
| 붙인 후 | **25,264 B** |

**차이 0.** `--gc-sections` 가 미사용 `attachInterrupt` 를 걷어낸다. 벡터 핸들러
넷은 남지만(벡터 테이블이 참조하므로) 본문이 `nrfx_gpiote_irq_handler` 호출 한 줄뿐이고,
정렬 패딩에 흡수됐다.

`Wire` 때와 결과가 갈린 이유는 **전역 객체의 유무다.** `Wire` 는 전역 인스턴스라
생성자가 `.init_array` 에 들어가는데, 링커 스크립트가 그 섹션을 `KEEP` 한다
(`nordic/bsp/mdk/common/nrf_common.ld` 의 `KEEP (*(.init_array))`). KEEP 된 섹션은
`--gc-sections` 의 루트라서 생성자가 살고, 생성자가 살면 클래스 본문이 딸려 온다.
GPIOTE 쪽은 전역 객체 없이 함수뿐이라 안 부르면 통째로 사라진다.

→ **코어에 두는 것이 맞다.** 일반화하면 이렇다: *코어에 전역 객체를 두면 모든
스케치가 비용을 치르고, 함수만 두면 부르는 스케치만 치른다.*

`attachInterrupt` 를 실제로 쓰는 스케치(위 시험 스케치)는 27,540 B 였다.

---

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
