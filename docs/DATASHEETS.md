# 데이터시트 / 벤더 문서 받는 법

**Nordic 문서 사이트는 스크립트 접근을 막는다.** `curl` 은 브라우저 User-Agent 를
줘도 403 이고, 핀 배정표 같은 표는 JavaScript 로 렌더링돼서 문서 페이지를 긁어도
내용이 안 나온다. 그래서 **브라우저로 직접 받아야 한다.** 이 문서는 매번 어디서
무엇을 받아야 하는지 다시 찾지 않으려고 남긴다.

받은 PDF 는 `docs/datasheets/` 에 둔다. **gitignore 된다** — 용량이 크고 재배포는
우리 권리가 아니다.

```sh
mkdir -p docs/datasheets
# 브라우저에서 아래 번들을 열고 "Download PDF" 로 저장
```

---

## 1. 모델별 번들

문서 URL 규칙: `https://docs.nordicsemi.com/bundle/<번들 ID>/`

| 대상 | 번들 ID | 다루는 모델 | 언제 필요한가 |
|---|---|---|---|
| **Product Specification** | `ps_nrf54l15` | **nRF54L15 / nRF54L10 / nRF54L05** | 현재 지원 보드 3종 전부 |
| Product Specification | `ps_nrf54lm20a` | nRF54LM20A / nRF54LM20B | M6 (CLAUDE.md §4.2) |
| nRF54L15 DK 사용자 가이드 | `ug_nrf54l15_dk` | — | DK 커넥터·핀 헤더 확인용 |
| Errata | `errata_nRF54L15_EngB` | nRF54L15 Engineering B | anomaly 확인 (예: SPIM anomaly) |

**하나의 PS 가 여러 모델을 덮는다.** `ps_nrf54l15` 한 벌이면 L05 까지 커버되므로
NU54-DK(L05) / NU54V-DK(L15) / XIAO(L15) 를 모두 이걸로 본다.

## 2. 이 프로젝트에서 실제로 보는 페이지

| 무엇 | 경로 |
|---|---|
| **핀 배정표** (어느 핀이 어느 페리페럴 신호로 가는가) | PS → *Pin assignments* (`page/chapters/pin.html`) |
| GPIO 전기 특성 | PS → GPIO → Electrical specification |
| SPIM / UARTE / TWIM 레지스터 | PS → 해당 페리페럴 장 |
| FICR 레이아웃 | PS → FICR. 단 **MDK 헤더가 더 빠르고 정확하다** (`nrf54l15_types.h` 의 `NRF_FICR_INFO_Type`) |

> **핀 배정은 Pin Planner 웹 도구가 더 편하다.** Nordic 이 nRF54L 시리즈용으로
> 제공하며, 페리페럴을 고르면 쓸 수 있는 핀을 보여 준다.
> 검색: "nRF54L Pin Planner".

## 3. 문서 대신 쓸 수 있는 것 (더 빠른 경우가 많다)

PDF 를 여는 것보다 이쪽이 빠르거나 정확한 경우가 많다. 실제로 XIAO 보드를 붙일 때
결정적인 값 두 개를 여기서 얻었다 (`docs/HIL/M1-xiao.md` §4).

| 궁금한 것 | 어디를 보나 |
|---|---|
| 레지스터 오프셋·비트필드·구조체 | **MDK 헤더** — 저장소 안에 이미 있다 (`cores/nrf54l/nordic/bsp/mdk/`) |
| 보드의 크리스털 로드 용량, 핀 배정, 센서 주소 | **upstream Zephyr 보드 정의** — `zephyrproject-rtos/zephyr` `boards/<벤더>/<보드>/` |
| SoftDevice 예약 자원 (IRQ 우선순위, GRTC 채널) | `sdk-nrf-bm` 의 `nrf_sd_def.h` |
| 페리페럴 드라이버 사용법 | nrfx 소스와 그 헤더 주석 |

**새 보드를 붙일 때는 벤더 Zephyr 보드 정의를 먼저 찾아라.** 회로도만으로 알 수 없는
값이 거기 들어 있다 (`docs/HIL/M1-xiao.md` §4 참조).

## 4. 확인해 둔 것 — GPIO 전원 도메인 규칙

`docs/PERIPHERAL-PINMAP.md` 의 근거다. Nordic 이 직접 쓴 DevZone 글에서 확인했다
("Essential pin planning guidelines for the nRF54L Series").

원문 그대로:

> Rule 1: "Generally, peripherals must use pins in their own power domain."
>
> "Selected pins on P2 can also be used by certain serial interfaces (SPIS, UARTE)
> located in PERI, although this configuration is less power-efficient."
>
> Rule 2: "Some peripherals with clock signals (like SPI, TWI, and TRACE) require
> the use of specific dedicated clock pins."
>
> Rule 4: 전용 핀만 쓰는 페리페럴 — FLPR, SPIM00/UARTE00, GRTC, TAMPC, NFC,
> RADIO direction-finding.

인스턴스 번호가 도메인을 정한다: `x00` → MCU 도메인(P2), `x20~x22` → PERI(P1),
`x30` → LP(P0).

**"selected pins on P2" 가 정확히 어느 핀인지는 아직 확인하지 못했다.**
PS 의 핀 배정표를 봐야 하는데 스크립트로는 못 가져온다. 브라우저나 Pin Planner 로
확인하고 `docs/PERIPHERAL-PINMAP.md` 에 채울 것.

## 5. 링크

- nRF54L15/L10/L05 PS — https://docs.nordicsemi.com/bundle/ps_nrf54l15/
- 핀 배정 — https://docs.nordicsemi.com/r/bundle/ps_nrf54l15/page/chapters/pin.html
- nRF54LM20A/B PS — https://docs.nordicsemi.com/r/bundle/ps_nrf54lm20a
- 핀 계획 가이드 (도메인 규칙 근거) — https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/essential-pin-planning-guidelines-for-the-nrf54l-series
- GPIO 포트와 핀 계획 (Academy) — https://academy.nordicsemi.com/courses/nrf54l-series-express-course/lessons/lesson-2-power-domains-event-system-and-gpio/topic/gpio-ports-and-pin-planning/
