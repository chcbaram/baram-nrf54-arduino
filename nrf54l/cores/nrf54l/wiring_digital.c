/*
 * wiring_digital.c — GPIO (nRF54L15)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit_nRF52_Arduino cores/nRF5/wiring_digital.c (LGPL-2.1) 의 API 를
 * 따르되, nrfx HAL 기반으로 다시 구현했다. nRF54L 은 포트가 3개라
 * nRF52 용 코드를 그대로 쓸 수 없다.
 */

#include "Arduino.h"
#include "WVariant.h"

#include <hal/nrf_gpio.h>

/* ─────────────────────────────────────────────────────────────────────
 * 핀 번호 → 절대 GPIO 번호
 * 매핑에 없거나 NC 로 표시된 핀은 무시한다(잘못된 레지스터 접근 방지).
 * ───────────────────────────────────────────────────────────────────── */
static inline bool pin_resolve(uint32_t arduino_pin, uint32_t *out_abs)
{
    if (arduino_pin >= PINS_COUNT) {
        return false;
    }
    uint32_t abs = g_ADigitalPinMap[arduino_pin];
    if (abs == NRF54L_PIN_NC) {
        return false;
    }
    *out_abs = abs;
    return true;
}

void pinMode(uint32_t dwPin, uint32_t dwMode)
{
    uint32_t pin;
    if (!pin_resolve(dwPin, &pin)) {
        return;
    }

    switch (dwMode)
    {
        case INPUT:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
            break;

        case INPUT_PULLUP:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
            break;

        case INPUT_PULLDOWN:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLDOWN);
            break;

        /* SENSE 계열은 System OFF 기상과 GPIOTE PORT 이벤트에 쓰인다. */
        case INPUT_PULLUP_SENSE:
            nrf_gpio_cfg_sense_input(pin, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
            break;

        case INPUT_PULLDOWN_SENSE:
            nrf_gpio_cfg_sense_input(pin, NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_SENSE_HIGH);
            break;

        case INPUT_SENSE_HIGH:
            nrf_gpio_cfg_sense_input(pin, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_HIGH);
            break;

        case INPUT_SENSE_LOW:
            nrf_gpio_cfg_sense_input(pin, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_LOW);
            break;

        case OUTPUT: /* == OUTPUT_S0S1 */
            nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
                         NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0S1, NRF_GPIO_PIN_NOSENSE);
            break;

        case OUTPUT_H0S1:
            nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
                         NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0S1, NRF_GPIO_PIN_NOSENSE);
            break;

        case OUTPUT_S0H1:
            nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
                         NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0H1, NRF_GPIO_PIN_NOSENSE);
            break;

        /* 고전류 구동. nRF54L 에서 SPIM00 등 고속 신호에 필요하다
         * (CLAUDE.md §4 SPI 주의사항의 E0/E1 출력 드라이브). */
        case OUTPUT_H0H1:
            nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
                         NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
            break;

        case OUTPUT_D0S1:
            nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
                         NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_D0S1, NRF_GPIO_PIN_NOSENSE);
            break;

        case OUTPUT_D0H1:
            nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
                         NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_D0H1, NRF_GPIO_PIN_NOSENSE);
            break;

        case OUTPUT_S0D1:
            nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
                         NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_S0D1, NRF_GPIO_PIN_NOSENSE);
            break;

        case OUTPUT_H0D1:
            nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
                         NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0D1, NRF_GPIO_PIN_NOSENSE);
            break;

        /* nRF54L 확장. nRF52 에는 없는 드라이브 강도다. */
        case OUTPUT_E0E1:
            nrf_gpio_cfg(pin, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
                         NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_E0E1, NRF_GPIO_PIN_NOSENSE);
            break;

        default:
            break;
    }
}

void digitalWrite(uint32_t dwPin, uint32_t dwVal)
{
    uint32_t pin;
    if (!pin_resolve(dwPin, &pin)) {
        return;
    }

    if (dwVal) {
        nrf_gpio_pin_set(pin);
    } else {
        nrf_gpio_pin_clear(pin);
    }
}

int digitalRead(uint32_t ulPin)
{
    uint32_t pin;
    if (!pin_resolve(ulPin, &pin)) {
        return LOW;
    }

    /* 출력으로 설정된 핀은 OUT 레지스터를, 입력이면 IN 레지스터를 읽어야
     * 한다. nrf_gpio_pin_read()는 방향을 보고 알아서 고른다. */
    return nrf_gpio_pin_read(pin) ? HIGH : LOW;
}

void digitalToggle(uint32_t dwPin)
{
    uint32_t pin;
    if (!pin_resolve(dwPin, &pin)) {
        return;
    }
    nrf_gpio_pin_toggle(pin);
}

/* ─────────────────────────────────────────────────────────────────────
 * LED 헬퍼 — variant 의 LED_STATE_ON 을 반영한다
 * NU54-DK 는 active HIGH 이므로 LED_STATE_ON = 1 이다 (docs/boards/NU54-DK.md).
 * ───────────────────────────────────────────────────────────────────── */
void ledOn(uint32_t pin)
{
    digitalWrite(pin, LED_STATE_ON);
}

void ledOff(uint32_t pin)
{
    digitalWrite(pin, !LED_STATE_ON);
}
