# nrfx vendoring 내역

원본: [NordicSemiconductor/nrfx](https://github.com/NordicSemiconductor/nrfx) **v4.5.0** (BSD-3-Clause)

## 왜 일부 파일을 지웠나

Arduino 빌드 시스템은 **`cores/` 아래의 모든 `.c` / `.cpp` / `.S` 를 무조건 컴파일한다.**
개별 파일을 빌드에서 빼는 방법이 없다.

nrfx 드라이버 대부분은 `#if NRFX_CHECK(NRFX_xxx_ENABLED)` 로 감싸져 있어 해당 SoC 에
없는 페리페럴이면 빈 오브젝트가 되지만, **일부는 그 가드가 없다** (`nrfx_adc.c` 등).
그런 파일은 nRF54L 에 없는 레지스터를 참조해 **빌드를 깨뜨린다.**

그래서 nRF54L 에서 컴파일되지 않는 소스를 제거했다.
판정은 추측이 아니라 **전부 `-fsyntax-only` 로 컴파일해 보고 실패한 것만** 골랐다.

## 제거한 소스 (21개)

`drivers/src/`
```
nrfx_adc.c          nRF51 ADC (nRF54L 은 SAADC)
nrfx_bellboard.c    nRF54H
nrfx_clock.c        구형 CLOCK (nRF54L 은 nrfx_clock_lfclk.c 등 분할본 사용)
nrfx_ipc.c          nRF53 / nRF54H
nrfx_mramc.c        nRF54H MRAM
nrfx_nvmc.c         nRF52 flash (nRF54L 은 RRAMC)
nrfx_power.c        nRF52 POWER
nrfx_qspi.c         nRF52840
nrfx_rng.c          nRF52 RNG (nRF54L 은 CRACEN)
nrfx_rtc.c          nRF52 RTC (nRF54L 은 GRTC)
nrfx_rtc_legacy.c   위와 동일
nrfx_spi.c          레거시 SPI (nRF54L 은 SPIM 만)
nrfx_tbm.c
nrfx_tdm.c          nRF54LM20A 전용. 코어에 TDM API 가 없어 아직 넣지 않았다
nrfx_twi.c          레거시 TWI
nrfx_uart.c         레거시 UART
nrfx_usbd.c         nRF52840 USB (nRF54LM20A 는 USBHS 로 다른 IP)
nrfx_usbreg.c       nRF52840
nrfx_vevif.c        nRF54H VPR
```

`helpers/`
```
nrfx_gppiv1_ipct.c
nrfx_memconf_trim.c
```

## 서로 배타적인 대안 (하나만 남겨야 한다)

nrfx 는 같은 심볼을 정의하는 **대안 구현**을 함께 담고 있고, 빌드 시스템이
그중 하나를 고르길 기대한다. Arduino 는 전부 컴파일하므로 직접 골라야 한다.
`--whole-archive`(platform.txt 참조)를 쓰면 중복 정의로 링크가 실패해 바로 드러난다.

| 제거 | 남긴 것 | 이유 |
|---|---|---|
| `bsp/mdk/common/startup_nrf_common.c` | `bsp/mdk/nrf54l/nrf54l15/gcc_startup_nrf54l15_application.S` | 둘 다 `Reset_Handler` / `Default_Handler` 정의. `.S` 쪽이 실기 검증됨 |
| `helpers/internal/nrfx_gppiv1_shim.c` | `helpers/nrfx_gppi_dppi.c` | GPPI 대안 구현. nRF54L 은 DPPI |
| `helpers/nrfx_gppi_ppi.c` | 〃 | 구형 PPI 용 |
| `drivers/src/nrfx_lpcomp.c` | `drivers/src/nrfx_comp.c` | **COMP 와 LPCOMP 가 IRQ 를 공유**해 `COMP_LPCOMP_IRQHandler` 를 둘 다 정의한다. 동시에 못 쓴다 |

## 그 밖에 제외한 것

- `bsp/` 는 `stable/` 만, 그중 `mdk/nrf54l/nrf54l15`·`mdk/nrf54l/nrf54lm20a` 와 공통 파일만 남겼다
  (전체는 301MB. 다른 nRF 계열 전부를 담고 있다)
- `mdk` 의 `.svd` 파일과 FLPR 스타트업 (`gcc_startup_nrf54l15_flpr.S`) 제거.
  FLPR 은 범위 밖이다 (CLAUDE.md R6). 단 `nrf54l15_flpr.h` 는 `nrf54l15.h` 가
  무조건 include 하므로 남겼다
- `doc/` 제거

## 업스트림을 올릴 때

1. 새 nrfx 를 받아 위 구조로 다시 추린다
2. **위 21개 목록을 그대로 믿지 말고** `-fsyntax-only` 판정을 다시 돌려라.
   nrfx 버전에 따라 가드가 추가/제거된다
3. 아래 "칩 가드 패치" 세 파일에 가드를 **다시 씌운다.** 새 원본으로 덮으면 사라지고,
   빠뜨리면 L15 와 LM20A 중 한쪽 빌드가 깨진다
4. LM20A 에 TDM API 를 넣게 되면 `nrfx_tdm.c` 를 되살리고 `NRF54LM20A_XXAA` 가드를 확인한다

판정 스크립트 예:

```sh
for f in $(find nordic -name '*.c'); do
  arm-none-eabi-gcc -fsyntax-only "$f" -mcpu=cortex-m33 -mthumb \
    -mfloat-abi=hard -mfpu=fpv5-sp-d16 -DNRF54L15_XXAA -DNRF_APPLICATION \
    <include 경로들> 2>/dev/null || echo "FAIL $f"
done
```

## nRF54LM20A 추가 (2026-09-14)

XIAO nRF54LM20A 지원 때 **같은 nrfx v4.5.0** 에서 가져왔다. 기존 L15 파일이 v4.5.0 원본과
바이트 단위로 같은 것을 먼저 확인했다 (`nrf54l15_application.h`, irqs/config 템플릿).

| 추가 | 원본 경로 |
|---|---|
| `bsp/mdk/nrf54l/nrf54lm20a/` (18 파일) | `bsp/stable/mdk/nrf54l/nrf54lm20a/` — L15 와 같은 기준으로 `.svd` 와 FLPR 스타트업은 제외 |
| `bsp/soc/irqs/nrfx_irqs_nrf54lm20a_application.h` | `bsp/stable/soc/irqs/` |
| `bsp/templates/nrfx_config_nrf54lm20a_application.h` | `bsp/stable/templates/` |

SoftDevice hex 는 nrfx 가 아니라 sdk-nrf-bm v2.0.1 에서 왔다 (`docs/LICENSE-INVENTORY.md`).

### 칩 가드 패치 — 원본과 다른 곳은 이 셋뿐이다

Arduino 는 `cores/` 아래를 전부 컴파일하므로 **칩별 파일을 빌드에서 뺄 수 없다.**
그래서 파일 머리에 칩 define 가드를 씌웠다. 각 파일 첫 주석에 같은 설명이 있다.

| 파일 | 가드 | 이유 |
|---|---|---|
| `bsp/mdk/nrf54l/nrf54l15/gcc_startup_nrf54l15_application.S` | `#if !defined(NRF54LM20A_XXAA)` | 스타트업 두 벌이 `Reset_Handler`·벡터 테이블을 중복 정의한다. L05 는 L15 스타트업을 쓴다 |
| `bsp/mdk/nrf54l/nrf54lm20a/gcc_startup_nrf54lm20a_application.S` | `#if defined(NRF54LM20A_XXAA)` | 위와 같다. LM20A 는 벡터가 다르다(SERIAL23/24, TDM, USBHS) |
| `drivers/src/nrfx_i2s.c` | `#if !defined(NRF54LM20A_XXAA)` | **LM20A 에는 I2S 가 없다** (TDM 으로 대체). MDK 에 `NRF_I2S_Type` 이 없어 HAL 헤더를 include 하는 순간 167줄 오류가 난다. `NRFX_I2S_ENABLED` 가드는 include 뒤에 있어 소용없다 |

`.S` 는 `-x assembler-with-cpp` 로 조립되고 `platform.txt` 가 칩 define 을 넘기므로
`#if` 가 통한다.
