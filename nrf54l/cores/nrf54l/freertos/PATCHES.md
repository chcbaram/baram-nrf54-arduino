# FreeRTOS-Kernel 로컬 수정 내역

원본: [FreeRTOS/FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) **V11.3.1** (MIT)

업스트림을 갱신할 때 아래 수정을 다시 적용해야 한다.
그 외 파일은 손대지 않았다.

---

## 1. `portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c`

`SVC_Handler` 정의 전체를 `#if !defined( configOVERRIDE_SVC_HANDLER )` 로 감쌌다.
**코드를 지우거나 고치지 않고 조건부로만 만들었다.**

### 왜

nRF54L 은 nRF52 와 구조가 반대다. 애플리케이션이 벡터 테이블을 소유하고,
SoftDevice 가 필요한 예외를 애플리케이션에서 포워딩받는다.
SoftDevice 도 SVC 를 쓰므로 `SVC_Handler` 를 하나만 둘 수 없고, SVC 번호로 갈라야 한다.

`s145_API/include/nrf_svc.h` 원문:

> The SVCs with SVC numbers 0x00-0x0F are forwarded to the application.
> All other SVCs are handled by the SoftDevice.

- `SDM_SVC_BASE = 0x10` (`nrf_sdm.h`), `SOC_SVC_BASE = 0x20` (`nrf_soc.h`)

문제가 **두 가지**다.

1. **번호 충돌** — FreeRTOS 11.3.1 은 `portSVC_*` 를 100~105 로 정의한다.
   전부 0x10 이상이라 디스패처가 SoftDevice 로 넘겨 버린다. → 아래 §2 에서 0~5 로 옮겼다
2. **심볼 충돌** — 포트의 `portasm.c` 와 우리 디스패처가 둘 다 `SVC_Handler` 를
   정의하면 링크가 실패한다. → 포트 쪽을 조건부로 끈다 (이 절)

⚠ 이 절에는 원래 "`ARM_CM33_NTZ` 는 `portSVC_START_SCHEDULER = 0` 만 쓰므로
**번호 충돌은 없다**" 고 적혀 있었다. **틀렸다.** 구버전 FreeRTOS 의 값(0~4)을
그대로 옮겨 적은 것이고, 실기에서 §2 의 증상으로 드러났다.

대체 구현은 `cores/nrf54l/nordic/sd_svc_dispatch.S` 이며, 포트의 `SVC_Handler` 가 하던 일
(`r0` = 스택 프레임 포인터 → `vPortSVCHandler_C` 로 분기)을 그대로 하고
번호 분기만 앞에 붙인다.

`configOVERRIDE_SVC_HANDLER` 는 `freertos/config/FreeRTOSConfig.h` 에서 정의한다.

참고: CLAUDE.md §7 F1

---

## 2. `portable/GCC/ARM_CM33_NTZ/non_secure/portmacrocommon.h`

`portSVC_*` 번호를 **100~105 → 0~5** 로 옮겼다.

### 왜

`nrf_svc.h` 가 SVC **0x10 이상을 전부 SoftDevice 몫**으로 규정한다.
FreeRTOS 11.3.1 은 이 값들을 100~105 로 정의하는데(구버전은 0~4 였다),
그대로 두면 FreeRTOS 의 SVC 가 SoftDevice 로 흘러간다.

**실기에서 실제로 겪은 증상**: `vStartFirstTask` 의 `svc 102` 가 디스패처의
SoftDevice 분기를 타고, `softdevice_vector_forward_address` 가 0 이라
`[0 + NRF_SD_ISR_OFFSET_SVC]` = 벡터[2] = `NMI_Handler` 로 점프해 무한루프.
폴트가 나지 않아 증상만으로는 원인을 알 수 없었다.

사용처가 전부 이 매크로를 거치므로 값만 바꾸면 된다.
`NUM_SYSTEM_CALLS` 는 MPU 경로 전용이라 우리 설정(`configENABLE_MPU=0`)에서는 무관하다.

> ⚠ FreeRTOS 를 업그레이드하면 이 번호가 또 바뀌었는지 **반드시** 확인하라.
> 11.x 에서 한 번 옮겨진 전례가 있다.

---

## 3. 패치가 **필요 없었던** 것

기록해 둔다. 나중에 불필요한 패치를 만들지 않기 위해서다.

| 훅 | 상태 |
|---|---|
| `vPortSetupTimerInterrupt()` | `port.c:847` 에서 **weak**. GRTC 틱으로 그냥 오버라이드하면 된다 |
| `vPortSuppressTicksAndSleep()` | `port.c:628` 에서 **weak**. tickless 구현을 그냥 오버라이드하면 된다 |

둘 다 `freertos/port_grtc.c` 에서 재정의한다.
SysTick 은 `vPortSetupTimerInterrupt()` 를 대체하는 순간 아예 켜지지 않으므로,
포트에 남아 있는 `SysTick_Handler` 는 호출되지 않는다.

---

## 4. 포함 범위

전체를 넣지 않았다.

| 포함 | 비고 |
|---|---|
| `tasks.c` `queue.c` `list.c` `timers.c` `event_groups.c` `stream_buffer.c` `croutine.c` | 커널 |
| `include/` | 공개 헤더 |
| `portable/GCC/ARM_CM33_NTZ/non_secure/` | TrustZone 미사용 포트 (CLAUDE.md R5 / §7 F4) |
| `portable/MemMang/heap_3.c` | newlib malloc 위임 |
| `portable/Common/` | |

`ARM_CM33`(TrustZone 사용) 포트와 secure 쪽은 넣지 않았다.
