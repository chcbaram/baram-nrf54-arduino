/*
 * wiring.c — 부팅 초기화와 저전력 (nRF54L15)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit_nRF52_Arduino cores/nRF5/wiring.c 의 API 를 따르되,
 * SoftDevice 의존부를 전부 하드웨어 직접 접근으로 바꿨다.
 * S145 에는 sd_app_evt_wait / sd_power_system_off / nrf_nvic.h 가 없다
 * (CLAUDE.md §6.1).
 *
 * System OFF 시퀀스는 sdk-nrf-bm v2.0.1
 * samples/bluetooth/ble_pwr_profiling/src/main.c 의 poweroff() 를 따랐다.
 */

#include "Arduino.h"
#include "WVariant.h"

#include <nrfx.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_regulators.h>
/* 리셋 원인 레지스터는 SoC 마다 다르다. nRF54L15 는 NRF_RESET 이고
 * nRF54H/nRF92 계열이 NRF_RESETINFO 다. nrfx_reset_reason.h 가 그 분기를
 * 알아서 하므로 hal 헤더를 직접 include 하지 않는다. */
#include <hal/nrf_temp.h>
#include <helpers/nrfx_reset_reason.h>
#include <helpers/nrfx_ram_ctrl.h>

#ifdef SOFTDEVICE_PRESENT
  #include "nrf_sdm.h"
  #include "nrf_soc.h"
#endif

static uint32_t _reset_reason = 0;

/* 링커 스크립트(MDK nrf_common.ld)가 주는 벡터 테이블 시작 심볼 */
extern uint32_t __vectors_start;

void init(void)
{
    /*
     * 벡터 테이블 위치를 명시한다.
     *
     * ⚠ MDK 스타트업은 VTOR 을 건드리지 않는다. 지금은 앱이 0x0 에 있어
     *   VTOR 기본값 0 으로 우연히 맞지만, M4 에서 부트로더가 들어오면
     *   상황이 뒤집힌다.
     *
     *   nRF54L 에는 MBR 도 UICR.BOOTLOADERADDR 도 없어서 CPU 가 0x0 에서
     *   바로 부팅한다. 따라서 **부트로더가 0x0 을 차지하고 앱이 위로 밀린다**
     *   (nRF52 와 정반대다. nRF52 는 MBR 이 0x0 에 있고 SoftDevice 가 그 위였다).
     *   그때 VTOR 을 안 옮기면 앱의 인터럽트가 부트로더 벡터로 간다.
     *
     *   링커 심볼에서 가져오므로 앱 시작 주소가 바뀌어도 자동으로 따라간다.
     */
    SCB->VTOR = (uint32_t)&__vectors_start;
    __DSB();
    __ISB();

    /* 리셋 원인을 읽어 두고 지운다. 지우지 않으면 다음 부팅에 섞인다. */
    _reset_reason = nrfx_reset_reason_get();
    nrfx_reset_reason_clear(_reset_reason);

    /*
     * LFCLK 소스. NU54-DK 는 32.768 kHz 크리스털(Y1)을 P1.00/P1.01 에
     * 달고 있으므로 LFXO 를 쓴다 (docs/boards/NU54-DK.md).
     * variant 에서 USE_LFXO / USE_LFRC 중 하나를 반드시 정의해야 한다.
     *
     * 실제 LFCLK 기동과 GRTC 클럭 선택은 FreeRTOS 틱 초기화
     * (freertos/port_grtc.c 의 vPortSetupTimerInterrupt)에서 한다.
     * 여기서 미리 켜면 GRTC 준비 전에 카운터가 돌아 첫 틱이 빗나간다.
     */
#if !defined(USE_LFXO) && !defined(USE_LFRC)
  #error "variant.h 에서 USE_LFXO 또는 USE_LFRC 를 정의해야 한다"
#endif

    /*
     * BusFault / MemManage / UsageFault 를 개별 예외로 승격한다.
     * 켜지 않으면 전부 HardFault 로 뭉뚱그려져 CFSR 만으로 원인을 좁히기
     * 어렵다. 핸들러는 cores/nrf54l/fault_handler.c 에 있다.
     */
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk
                | SCB_SHCSR_MEMFAULTENA_Msk
                | SCB_SHCSR_USGFAULTENA_Msk;
}

uint32_t readResetReason(void)
{
    return _reset_reason;
}

void waitForEvent(void)
{
    /* SoftDevice 유무와 무관하게 그냥 잔다.
     * SD 의 IRQ 는 우선순위 0/4 로 계속 살아 있으므로 알아서 깨운다. */
    __SEV();
    __WFE();
    __WFE();
}

void systemOff(uint32_t pin, uint8_t wake_logic)
{
    uint32_t abs_pin;

    if (pin >= PINS_COUNT || g_ADigitalPinMap[pin] == NRF54L_PIN_NC) {
        return;
    }
    abs_pin = g_ADigitalPinMap[pin];

    /* 1) 기상 핀 설정. SENSE 가 걸린 핀만 System OFF 를 깨울 수 있다. */
    if (wake_logic) {
        nrf_gpio_cfg_sense_input(abs_pin, NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_SENSE_HIGH);
    } else {
        nrf_gpio_cfg_sense_input(abs_pin, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
    }

    /*
     * 2) RAM 리텐션 해제.
     *
     * System OFF 전류를 좌우하는 부분이다. Adafruit 은 nRF52 에서
     * 이 처리(NRF_POWER->RAM[i].POWERCLR)를 주석 처리한 채 두었지만,
     * Nordic 자신의 ble_pwr_profiling 샘플은 반드시 수행한다.
     *
     * System OFF 기상은 리셋이므로 RAM 내용을 보존할 이유가 없다.
     * 단 M4 에서 .noinit 플래그로 부트로더 진입을 구현하면 그 영역만
     * 리텐션을 남겨야 한다 (CLAUDE.md §7 F7 방법 2).
     */
    nrfx_ram_ctrl_retention_enable_set((void *)NRF_MEMORY_RAM_BASE,
                                       NRF_MEMORY_RAM_SIZE,
                                       false);

    /* 3) 리셋 원인을 지운다. 지우지 않으면 기상 후 판정이 섞인다. */
    nrfx_reset_reason_clear(UINT32_MAX);

    /* 4) 진입. 돌아오지 않는다.
     *    ⚠ SWD 가 붙어 있으면 nrf_regulators_system_off() 내부의
     *      while(1){__WFE();} 폴백으로 빠진다 (F8). */
    nrf_regulators_system_off(NRF_REGULATORS);
}

float readCPUTemperature(void)
{
#ifdef SOFTDEVICE_PRESENT
    uint8_t sd_en = 0;
    (void)sd_softdevice_is_enabled(&sd_en);
    if (sd_en) {
        int32_t temp = 0;
        /* SoftDevice 가 활성이면 TEMP 를 직접 만지면 안 된다. */
        (void)sd_temp_get(&temp);
        return temp * 0.25f;
    }
#endif
    nrf_temp_task_trigger(NRF_TEMP, NRF_TEMP_TASK_START);
    while (!nrf_temp_event_check(NRF_TEMP, NRF_TEMP_EVENT_DATARDY)) { }
    nrf_temp_event_clear(NRF_TEMP, NRF_TEMP_EVENT_DATARDY);

    int32_t raw = nrf_temp_result_get(NRF_TEMP);
    nrf_temp_task_trigger(NRF_TEMP, NRF_TEMP_TASK_STOP);

    return raw * 0.25f;
}
