/*
 * FreeRTOS 설정 — baram-nrf54l-arduino (NU54-DK / nRF54L15 + SoftDevice S145)
 * SPDX-License-Identifier: MIT
 *
 * 우선순위와 SVC 관련 값은 임의로 바꾸지 마라. 틀리면 원인 추적이 거의
 * 불가능한 랜덤 크래시로 나타난다. 근거는 아래 주석과 CLAUDE.md §7 F1/F2.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════
 * 클럭 / 틱
 * ═══════════════════════════════════════════════════════════════════ */
#define configCPU_CLOCK_HZ                      ( 128000000UL )

/*
 * 틱 소스는 SysTick 이 아니라 GRTC 다 (CLAUDE.md §7 F3).
 * SysTick 은 저전력 모드에서 멈추므로 tickless 에 쓸 수 없다.
 *
 * GRTC SYSCOUNTER 는 64비트 / 1 MHz (nrf_grtc.h:
 * NRF_GRTC_SYSCOUNTER_MAIN_FREQUENCY_HZ). 1 MHz 라 1000 Hz 틱이면
 * 틱당 정확히 1000 카운트로 떨어져 millis() 에 오차가 생기지 않는다.
 *
 * Adafruit nRF52 코어는 1024 Hz 를 쓰는데, 그건 RTC1 의 32768 Hz 를
 * 32 로 정수 분주하기 위한 값이었다. 여기서는 그럴 이유가 없다.
 *
 * 포트의 기본 SysTick 설정은 vPortSetupTimerInterrupt() 를 오버라이드해
 * 무력화한다. 그 함수는 포트에서 weak 로 선언돼 있어 패치가 필요 없다.
 * 구현은 freertos/port_grtc.c 에 있다.
 */
#define configTICK_RATE_HZ                      1000
#define configSYSTICK_CLOCK_HZ                  ( 1000000UL )   /* GRTC SYSCOUNTER */

/* ═══════════════════════════════════════════════════════════════════
 * 인터럽트 우선순위 — SoftDevice 공존 (CLAUDE.md §7 F2)
 * ═══════════════════════════════════════════════════════════════════
 *
 * 근거 1) __NVIC_PRIO_BITS = 3
 *         mdk/nrf54l/nrf54l15/nrf54l15_application.h:193
 *         → 우선순위는 0~7. 숫자가 작을수록 긴급하다.
 *
 * 근거 2) SoftDevice 가 점유하는 우선순위
 *         sdk-nrf-bm v2.0.1 subsys/softdevice_handler/irq_connect.c
 *
 *           우선순위 0 : RADIO_0, TIMER10, GRTC_3      (zero-latency)
 *           우선순위 4 : AAR00_CCM00, CLOCK_POWER,
 *                        ECB00, SWI00, SVCall          (non-time-critical)
 *
 *         우선순위 0 은 IRQ_ZERO_LATENCY 로 연결되며 SoftDevice 가
 *         sd_softdevice_enable() 에서 필요한 값으로 덮어쓴다.
 *
 * 결론:
 *   configMAX_SYSCALL_INTERRUPT_PRIORITY = 5
 *     SoftDevice 의 4 보다 "덜 긴급"해야 한다. FreeRTOS 가 BASEPRI 로
 *     이 값 이상(숫자상)을 마스크해도 SoftDevice 의 0/4 는 계속 돈다.
 *   configKERNEL_INTERRUPT_PRIORITY = 7 (최저)
 *     PendSV 와 GRTC 틱이 여기서 돈다.
 *
 * ⚠ 애플리케이션 ISR 에서 ...FromISR() 계열을 부르려면 그 ISR 의
 *   우선순위가 반드시 5, 6, 7 중 하나여야 한다. 0~4 에서 부르면 깨진다.
 *   nrfx 드라이버 기본값은 6 이다 (nordic/nrfx_config.h).
 *
 * ⚠ 이 포트는 두 값을 BASEPRI 레지스터에 그대로 쓴다(port.c:2213).
 *   따라서 미리 시프트해 둔 값이어야 한다: prio << (8 - 3).
 */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS                     __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS                     3
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY        7
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY   5

#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )        /* 0xE0 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )   /* 0xA0 */

/* ═══════════════════════════════════════════════════════════════════
 * SVC 핸들러 — SoftDevice 와 공존 (CLAUDE.md §7 F1)
 * ═══════════════════════════════════════════════════════════════════
 *
 * nRF54L 은 애플리케이션이 벡터 테이블을 소유하고 SoftDevice 로 예외를
 * 포워딩하는 구조다. SoftDevice 도 SVC 를 쓰므로 SVC_Handler 를 하나만
 * 둘 수 없다. SVC 번호로 갈라야 한다.
 *
 *   nrf_svc.h : "SVC numbers 0x00-0x0F are forwarded to the application.
 *                All other SVCs are handled by the SoftDevice."
 *   SDM_SVC_BASE = 0x10, SOC_SVC_BASE = 0x20
 *   FreeRTOS ARM_CM33_NTZ 는 portSVC_START_SCHEDULER = 0 만 쓴다 → 충돌 없음
 *
 * 아래 매크로로 포트(portasm.c)의 SVC_Handler 정의를 끄고,
 * cores/nrf54l/nordic/sd_svc_dispatch.S 의 디스패처를 쓴다.
 * portasm.c 에 가한 패치는 freertos/PATCHES.md 에 기록돼 있다.
 */
#define configOVERRIDE_SVC_HANDLER              1

/* ═══════════════════════════════════════════════════════════════════
 * ARMv8-M 포트 옵션
 * ═══════════════════════════════════════════════════════════════════ */
/* TrustZone 미사용, secure-only 고정 (CLAUDE.md R5).
 * ARM_CM33_NTZ 포트를 쓰는 이유가 이것이다. */
#define configENABLE_TRUSTZONE                  0
#define configRUN_FREERTOS_SECURE_ONLY          1
#define configENABLE_MPU                        0
#define configENABLE_FPU                        1
#define configENABLE_MVE                        0

/* ═══════════════════════════════════════════════════════════════════
 * 스케줄러
 * ═══════════════════════════════════════════════════════════════════ */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configIDLE_SHOULD_YIELD                 1

/* Adafruit rtos.h 의 TASK_PRIO_LOWEST(0) ~ TASK_PRIO_HIGHEST(4) 와 맞춘다.
 * 스케치 호환의 핵심이므로 줄이지 마라 (CLAUDE.md §6). */
#define configMAX_PRIORITIES                    ( 5 )

#define configMINIMAL_STACK_SIZE                ( ( uint16_t ) 128 )
#define configMAX_TASK_NAME_LEN                 ( 16 )
/* configUSE_16_BIT_TICKS 는 쓰지 않는다. 둘 다 정의하면 FreeRTOS.h 가 막는다. */
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS

/* ═══════════════════════════════════════════════════════════════════
 * 메모리
 * ═══════════════════════════════════════════════════════════════════
 * heap_3 = newlib malloc 위임. 링커 스크립트에서 __HEAP_SIZE=0 으로 두고
 * _sbrk 가 __HeapBase ~ __StackLimit 전체를 힙으로 쓴다.
 * 태스크 스택도 전부 여기서 나온다 (MSP 는 ISR/SoftDevice 전용).
 */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         1
#define configAPPLICATION_ALLOCATED_HEAP        0

/* ═══════════════════════════════════════════════════════════════════
 * 저전력 — tickless idle (CLAUDE.md §6.1)
 * ═══════════════════════════════════════════════════════════════════
 *
 * ⚠ 지금은 꺼 둔다. 틱이 먼저 안정된 뒤에 켜야 한다.
 *   둘을 동시에 켜면 틱 버그와 슬립 버그가 섞여 원인 분리가 불가능하다.
 *   켤 때 port_grtc.c 의 vPortSuppressTicksAndSleep() 이 쓰인다
 *   (포트의 것은 weak 라 자동으로 대체된다).
 */
#define configUSE_TICKLESS_IDLE                 0
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP   2
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

/* ═══════════════════════════════════════════════════════════════════
 * 기능 스위치
 * ═══════════════════════════════════════════════════════════════════ */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0
#define configUSE_TASK_NOTIFICATIONS            1
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configUSE_CO_ROUTINES                   0
#define configUSE_NEWLIB_REENTRANT              0
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( 2 )
#define configTIMER_QUEUE_LENGTH                8
#define configTIMER_TASK_STACK_DEPTH            ( 256 )

#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* ═══════════════════════════════════════════════════════════════════
 * API 포함 여부
 * ═══════════════════════════════════════════════════════════════════ */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xQueueGetMutexHolder            1
#define INCLUDE_xSemaphoreGetMutexHolder        1

/* ═══════════════════════════════════════════════════════════════════
 * assert
 * ═══════════════════════════════════════════════════════════════════ */
/* rtos.cpp 가 extern "C" 안에서 정의하므로 링키지를 맞춰야 한다.
 * 이 헤더는 C 와 C++ 양쪽에서 include 된다. */
#ifdef __cplusplus
extern "C" {
#endif
extern void vAssertCalled( const char * file, int line );
#ifdef __cplusplus
}
#endif
#define configASSERT( x )  if( ( x ) == 0 ) { vAssertCalled( __FILE__, __LINE__ ); }

#endif /* FREERTOS_CONFIG_H */
