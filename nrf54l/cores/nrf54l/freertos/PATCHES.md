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
- FreeRTOS `ARM_CM33_NTZ` 는 `portSVC_START_SCHEDULER = 0` 만 쓴다 → **번호 충돌은 없다**

문제는 심볼 충돌이다. 포트의 `portasm.c` 와 우리 디스패처가 둘 다 `SVC_Handler` 를
정의하면 링크가 실패한다. 그래서 포트 쪽을 조건부로 끈다.

대체 구현은 `cores/nrf54l/nordic/sd_svc_dispatch.S` 이며, 포트의 `SVC_Handler` 가 하던 일
(`r0` = 스택 프레임 포인터 → `vPortSVCHandler_C` 로 분기)을 그대로 하고
번호 분기만 앞에 붙인다.

`configOVERRIDE_SVC_HANDLER` 는 `freertos/config/FreeRTOSConfig.h` 에서 정의한다.

참고: CLAUDE.md §7 F1

---

## 2. 패치가 **필요 없었던** 것

기록해 둔다. 나중에 불필요한 패치를 만들지 않기 위해서다.

| 훅 | 상태 |
|---|---|
| `vPortSetupTimerInterrupt()` | `port.c:847` 에서 **weak**. GRTC 틱으로 그냥 오버라이드하면 된다 |
| `vPortSuppressTicksAndSleep()` | `port.c:628` 에서 **weak**. tickless 구현을 그냥 오버라이드하면 된다 |

둘 다 `freertos/port_grtc.c` 에서 재정의한다.
SysTick 은 `vPortSetupTimerInterrupt()` 를 대체하는 순간 아예 켜지지 않으므로,
포트에 남아 있는 `SysTick_Handler` 는 호출되지 않는다.

---

## 3. 포함 범위

전체를 넣지 않았다.

| 포함 | 비고 |
|---|---|
| `tasks.c` `queue.c` `list.c` `timers.c` `event_groups.c` `stream_buffer.c` `croutine.c` | 커널 |
| `include/` | 공개 헤더 |
| `portable/GCC/ARM_CM33_NTZ/non_secure/` | TrustZone 미사용 포트 (CLAUDE.md R5 / §7 F4) |
| `portable/MemMang/heap_3.c` | newlib malloc 위임 |
| `portable/Common/` | |

`ARM_CM33`(TrustZone 사용) 포트와 secure 쪽은 넣지 않았다.
