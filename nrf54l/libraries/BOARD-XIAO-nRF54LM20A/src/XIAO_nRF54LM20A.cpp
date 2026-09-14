/*
 * XIAO_nRF54LM20A.cpp — XIAO nRF54LM20A / Sense 보드 전용
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "XIAO_nRF54LM20A.h"

#include <Wire.h>

#include "FreeRTOS.h"
#include "semphr.h"

/* ── 레지스터 (Zephyr nPM13xx 드라이버와 같은 이름) ──────────────────── */
#define VBUS_BASE              0x02
#define VBUS_OFFSET_STATUS     0x07
#define VBUS_STATUS_PRESENT    0x01

#define CHGR_BASE              0x03
#define CHGR_OFFSET_CHG_STAT   0x34
#define CHGR_OFFSET_ERR_REASON 0x36

#define ADC_BASE               0x05
#define ADC_OFFSET_TASK_VBAT   0x00
#define ADC_OFFSET_TASK_DIE    0x02
#define ADC_OFFSET_RESULTS     0x10
/* RESULTS 부터의 배치: ibat_stat, msb_vbat, msb_ntc, msb_die, msb_vsys, lsb_a */
#define ADC_RES_MSB_VBAT       1
#define ADC_RES_MSB_DIE        3
#define ADC_RES_LSB_A          5
#define ADC_LSB_VBAT_SHIFT     0
#define ADC_LSB_DIE_SHIFT      4
/* 변환 한 번 250 µs (Zephyr ADC_CONV_TIME_US). 넉넉히 기다린다. */
#define ADC_CONV_WAIT_MS       2

#define LDSW_BASE              0x08
#define LDSW_OFFSET_EN_SET     0x00    /* LDSW1. LDSW2 는 +2 */
#define LDSW_OFFSET_EN_CLR     0x01
#define LDSW_OFFSET_STATUS     0x04
#define LDSW_OFFSET_LDOSEL     0x08    /* 1 = LDO, 0 = 부하 스위치 */
#define LDSW_OFFSET_VOUTSEL    0x0C
#define LDSW1_ON_MASK          0x03

/* 버킹·LDO 공통 전압표: 1.0 V + 0.1 V × idx, idx 0~23 */
#define LDO_MV_MIN             1000
#define LDO_MV_MAX             3300
#define LDO_MV_STEP            100

/*
 * PMIC 전용 버스. Wire(TWIM22)·Wire1(TWIM30)과 별개인 TWIM24 다 (variant.h).
 * 다중 인스턴스 드라이버라 벡터를 직접 잇는다 (CLAUDE.md §7 F10 ③).
 */
static TwoWire s_bus(PMIC_TWIM_INSTANCE, PIN_PMIC_SDA, PIN_PMIC_SCL);

extern "C" void PMIC_TWIM_IRQ_HANDLER(void) { s_bus._irqHandler(); }

NPM1300 PMIC;

NPM1300::NPM1300(void) : _begun(false), _lock(NULL)
{
}

bool NPM1300::begin(void)
{
  if (!_begun) {
    /*
     * 레지스터 읽기는 "주소 쓰기 → 반복 시작 → 읽기" 두 프레임이다. 사이에 다른
     * 태스크의 전송이 끼면 엉뚱한 레지스터를 읽으므로 트랜잭션 전체를 락으로 묶는다.
     * 초기화 시점 한 번만 할당한다.
     */
    _lock = xSemaphoreCreateMutex();
    if (_lock == NULL) return false;

    s_bus.begin();
    _begun = true;
  }

  /* VBUS 상태 레지스터가 읽히면 PMIC 가 있는 것이다. */
  return readRegister(VBUS_BASE, VBUS_OFFSET_STATUS) >= 0;
}

bool NPM1300::setSensorPower(bool on, uint16_t mv)
{
  if (!_begun) return false;

  if (!on) {
    return writeRegister(LDSW_BASE, LDSW_OFFSET_EN_CLR, 1);
  }

  if (mv < LDO_MV_MIN || mv > LDO_MV_MAX || (mv % LDO_MV_STEP) != 0) return false;

  uint8_t idx = (uint8_t) ((mv - LDO_MV_MIN) / LDO_MV_STEP);

  /* 모드 → 전압 → enable. 켜진 채로 모드를 바꾸지 않도록 이 순서를 지킨다. */
  if (!writeRegister(LDSW_BASE, LDSW_OFFSET_LDOSEL, 1))    return false;
  if (!writeRegister(LDSW_BASE, LDSW_OFFSET_VOUTSEL, idx)) return false;
  return writeRegister(LDSW_BASE, LDSW_OFFSET_EN_SET, 1);
}

bool NPM1300::sensorPowerOn(void)
{
  int st = readRegister(LDSW_BASE, LDSW_OFFSET_STATUS);
  return st >= 0 && (st & LDSW1_ON_MASK) != 0;
}

bool NPM1300::vbusPresent(void)
{
  int st = readRegister(VBUS_BASE, VBUS_OFFSET_STATUS);
  return st >= 0 && (st & VBUS_STATUS_PRESENT) != 0;
}

int NPM1300::chargeStatus(void)
{
  return readRegister(CHGR_BASE, CHGR_OFFSET_CHG_STAT);
}

int NPM1300::chargeError(void)
{
  return readRegister(CHGR_BASE, CHGR_OFFSET_ERR_REASON);
}

int32_t NPM1300::batteryMillivolts(void)
{
  uint8_t r[ADC_RES_LSB_A + 1];

  if (!writeRegister(ADC_BASE, ADC_OFFSET_TASK_VBAT, 1)) return -1;
  delay(ADC_CONV_WAIT_MS);
  if (!readBurst(ADC_BASE, ADC_OFFSET_RESULTS, r, sizeof(r))) return -1;

  /* 10 비트 = MSB 8 비트 << 2 | LSB 2 비트. 0~5 V 풀스케일. */
  uint16_t code = ((uint16_t) r[ADC_RES_MSB_VBAT] << 2) |
                  ((r[ADC_RES_LSB_A] >> ADC_LSB_VBAT_SHIFT) & 0x03);
  return (int32_t) code * 5000 / 1024;
}

float NPM1300::dieTemperature(void)
{
  uint8_t r[ADC_RES_LSB_A + 1];

  if (!writeRegister(ADC_BASE, ADC_OFFSET_TASK_DIE, 1)) return NAN;
  delay(ADC_CONV_WAIT_MS);
  if (!readBurst(ADC_BASE, ADC_OFFSET_RESULTS, r, sizeof(r))) return NAN;

  uint16_t code = ((uint16_t) r[ADC_RES_MSB_DIE] << 2) |
                  ((r[ADC_RES_LSB_A] >> ADC_LSB_DIE_SHIFT) & 0x03);

  /* nPM1300 PS 7.1.4 (Zephyr calc_dietemp): 394.67 - code × 0.7926 °C */
  int32_t mdegc = 394670 - (int32_t) ((int32_t) code * 3963000 / 5000);
  return mdegc / 1000.0f;
}

int NPM1300::readRegister(uint8_t base, uint8_t offset)
{
  uint8_t v;
  return readBurst(base, offset, &v, 1) ? v : -1;
}

/*───────────────────────────────────────────────────────────────────*/

bool NPM1300::writeRegister(uint8_t base, uint8_t offset, uint8_t value)
{
  if (!_begun) return false;

  xSemaphoreTake((SemaphoreHandle_t) _lock, portMAX_DELAY);
  s_bus.beginTransmission(PMIC_I2C_ADDRESS);
  s_bus.write(base);
  s_bus.write(offset);
  s_bus.write(value);
  bool ok = (s_bus.endTransmission() == 0);
  xSemaphoreGive((SemaphoreHandle_t) _lock);

  return ok;
}

bool NPM1300::readBurst(uint8_t base, uint8_t offset, uint8_t *buf, size_t len)
{
  if (!_begun) return false;

  bool ok = false;

  xSemaphoreTake((SemaphoreHandle_t) _lock, portMAX_DELAY);
  s_bus.beginTransmission(PMIC_I2C_ADDRESS);
  s_bus.write(base);
  s_bus.write(offset);
  /* false 로 버스를 유지한다. STOP 이 끼면 PMIC 가 주소 포인터를 잃는다. */
  if (s_bus.endTransmission(false) == 0 &&
      s_bus.requestFrom(PMIC_I2C_ADDRESS, len) == len) {
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t) s_bus.read();
    ok = true;
  }
  xSemaphoreGive((SemaphoreHandle_t) _lock);

  return ok;
}
