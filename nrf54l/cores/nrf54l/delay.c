/*
 * delay.c — 시간 (nRF54L15 / GRTC)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */

#include "Arduino.h"

#include <nrfx.h>
#include <lib/nrfx_coredep.h>

/* freertos/port_grtc.c */
extern uint64_t nrf54l_syscounter_us(void);

uint32_t millis(void)
{
    /* configTICK_RATE_HZ = 1000 이므로 틱이 곧 밀리초다.
     * 나누어떨어지지 않는 틱 레이트를 쓰면 여기서 오차가 생긴다.
     * port_grtc.c 에 그걸 막는 #error 가 있다. */
    return (uint32_t)xTaskGetTickCount();
}

uint64_t micros64(void)
{
    return nrf54l_syscounter_us();
}

uint32_t micros(void)
{
    return (uint32_t)nrf54l_syscounter_us();
}

void delay(uint32_t ms)
{
    if (ms == 0) {
        return;
    }

    /* 스케줄러가 아직 안 돌면 vTaskDelay 를 쓸 수 없다.
     * setup() 이전(초기화 경로)에서 불릴 수 있으므로 방어한다. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        delayMicroseconds(ms * 1000UL);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(ms));
}

void delayMicroseconds(uint32_t us)
{
    if (us == 0) {
        return;
    }
    /* nrfx 의 코어 사이클 기반 지연. NRFX_DELAY_US 와 같다. */
    nrfx_coredep_delay_us(us);
}
