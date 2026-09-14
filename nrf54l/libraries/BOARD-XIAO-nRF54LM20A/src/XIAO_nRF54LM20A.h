/*
 * XIAO_nRF54LM20A.h — XIAO nRF54LM20A / Sense 보드 전용
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * nPM1300 PMIC 를 다룬다. 이 보드에서 PMIC 는 단순한 충전기가 아니라
 *
 *   VSYS_3V3     칩·LED 전원      BUCK2   — **건드리지 않는다**
 *   IMU&MIC_3V3  Sense 센서 전원  LDO1    — setSensorPower()
 *
 * 를 만든다. 그래서 Sense 의 IMU 를 쓰려면 이 라이브러리가 먼저다.
 *
 * 레지스터 주소와 변환식의 출처:
 *   Zephyr  drivers/regulator/regulator_npm13xx.c  (LDSW/LDO)
 *           drivers/sensor/nordic/npm13xx_charger/npm13xx_charger.c  (ADC, 충전)
 *   sdk-nrf samples/pmic/native/npm13xx_fuel_gauge  (BCHGCHARGESTATUS 비트)
 */
#ifndef XIAO_NRF54LM20A_H_
#define XIAO_NRF54LM20A_H_

#include <Arduino.h>

class NPM1300
{
  public:
    /** 충전 상태 (BCHGCHARGESTATUS). 비트 의미는 Nordic 샘플 기준이다. */
    enum : uint8_t {
      CHG_COMPLETE = 0x02,
      CHG_TRICKLE  = 0x04,
      CHG_CC       = 0x08,   /* 정전류 */
      CHG_CV       = 0x10,   /* 정전압 */
    };

    NPM1300(void);

    /**
     * 버스를 열고 PMIC 가 응답하는지 본다.
     * setup() 에서 한 번 부른다. 두 번 불러도 된다.
     * @return PMIC 가 응답하면 true
     */
    bool begin(void);

    /**
     * Sense 센서(IMU·마이크) 전원 = LDO1.
     *
     * 켤 때 LDO 모드와 전압을 먼저 맞추고 enable 한다 (Zephyr 와 같은 순서).
     * 전압 기본값 3.3 V 는 회로도의 레일 이름(IMU&MIC_3V3)을 따른 것이다.
     * ⚠ Zephyr 보드 정의는 1.8 V 로 켜지만, IMU 버스 풀업이 이 레일에 있어
     *   3.3 V 로 도는 nRF 가 1.8 V HIGH 를 논리 1 로 못 읽을 수 있다.
     *
     * @param on   true 면 켠다
     * @param mv   1000~3300, 100 mV 단위
     * @return 쓰기가 모두 성공하면 true
     */
    bool setSensorPower(bool on, uint16_t mv = 3300);

    /** LDO1 이 켜져 있으면 true. 읽기 실패도 false. */
    bool sensorPowerOn(void);

    /** VBUS(USB) 전원이 들어와 있으면 true. 읽기 실패는 false. */
    bool vbusPresent(void);

    /** 충전 상태 원시값 (CHG_* 비트). 실패하면 -1. */
    int  chargeStatus(void);

    /** 충전 오류 원인 원시값 (BCHGERRREASON). 0 이면 오류 없음. 실패하면 -1. */
    int  chargeError(void);

    /**
     * 배터리 전압 (mV). PMIC 의 10 비트 ADC, 0~5 V 범위.
     * 배터리가 없으면 의미 없는 값이 나온다. 실패하면 -1.
     */
    int32_t batteryMillivolts(void);

    /** PMIC 다이 온도 (°C). 실패하면 NAN. */
    float dieTemperature(void);

    /**
     * 레지스터 하나. nPM1300 은 주소가 (base, offset) 2 바이트다.
     * 실패하면 -1. 쓰기는 레귤레이터를 끌 수 있으니 공개하지 않는다.
     */
    int  readRegister(uint8_t base, uint8_t offset);

  private:
    bool writeRegister(uint8_t base, uint8_t offset, uint8_t value);
    bool readBurst(uint8_t base, uint8_t offset, uint8_t *buf, size_t len);

    bool  _begun;
    void *_lock;         /* SemaphoreHandle_t — 헤더에 FreeRTOS 를 끌어오지 않으려고 void* */
};

extern NPM1300 PMIC;

/* Sense 의 IMU. Arduino_LSM6DS3 과 같은 API — LSM6DS3TRC.h 참조. */
#include "LSM6DS3TRC.h"

#endif /* XIAO_NRF54LM20A_H_ */
