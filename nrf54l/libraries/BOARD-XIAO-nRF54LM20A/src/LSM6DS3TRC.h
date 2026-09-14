/*
 * LSM6DS3TRC.h — XIAO nRF54LM20A Sense 의 IMU (ST LSM6DS3TR-C)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Arduino 공식 IMU 라이브러리(Arduino_LSM6DS3 / Arduino_BMI270_BMM150)와
 * 같은 모양이다. 그 라이브러리로 짠 스케치가 include 한 줄만 바꾸면 돈다:
 *
 *   IMU.begin();
 *   if (IMU.accelerationAvailable()) IMU.readAcceleration(x, y, z);   // g
 *   if (IMU.gyroscopeAvailable())    IMU.readGyroscope(x, y, z);      // dps
 *
 * 기본값도 Arduino_LSM6DS3 과 같다 — 104 Hz, ±4 g, ±2000 dps.
 *
 * ⚠ 이 보드에서 IMU 전원은 GPIO 가 아니라 nPM1300 LDO1 이다. begin() 이
 *   PMIC 로 레일을 켠다. end() 는 IMU 만 끄고 레일은 두는데, 같은 레일에
 *   마이크가 있기 때문이다. 레일까지 끄려면 PMIC.setSensorPower(false).
 *
 * ⚠ 락은 이 클래스의 호출끼리만 막는다. TwoWire 자체에는 버스 락이 없으므로
 *   다른 코드가 다른 태스크에서 같은 Wire1 을 쓰면 트랜잭션이 섞일 수 있다.
 *
 * 레지스터·비트·감도는 ST 공식 드라이버(STMicroelectronics/lsm6ds3tr-c-pid)에서 가져왔다.
 */
#ifndef LSM6DS3TRC_H_
#define LSM6DS3TRC_H_

#include <Arduino.h>
#include <Wire.h>

class LSM6DS3TRC
{
  public:
    /** 출력 데이터 속도. CTRL1_XL / CTRL2_G 의 ODR 코드 그대로다. */
    enum Rate : uint8_t {
      RATE_OFF    = 0,
      RATE_12_5HZ = 1,
      RATE_26HZ   = 2,
      RATE_52HZ   = 3,
      RATE_104HZ  = 4,
      RATE_208HZ  = 5,
      RATE_416HZ  = 6,
      RATE_833HZ  = 7,
      RATE_1660HZ = 8,
      RATE_3330HZ = 9,
      RATE_6660HZ = 10,
    };

    /** 가속도 범위. 코드 순서가 크기 순이 아니다 (ST 정의 그대로). */
    enum AccelRange : uint8_t {
      ACCEL_2G  = 0,
      ACCEL_16G = 1,
      ACCEL_4G  = 2,
      ACCEL_8G  = 3,
    };

    /** 각속도 범위. fs_g(2 비트) + fs_125(1 비트)를 합친 3 비트 코드다. */
    enum GyroRange : uint8_t {
      GYRO_250DPS  = 0,
      GYRO_125DPS  = 1,
      GYRO_500DPS  = 2,
      GYRO_1000DPS = 4,
      GYRO_2000DPS = 6,
    };

    LSM6DS3TRC(TwoWire &wire, uint8_t address);

    /**
     * 센서 전원을 켜고 IMU 를 확인한 뒤 기본값으로 설정한다.
     * @return 1 이면 성공. 0 이면 IMU 가 없다 (Sense 가 아닌 보드)
     */
    int  begin(void);

    /** 가속도계·자이로를 끈다 (전원 레일은 둔다). */
    void end(void);

    bool setAccelerometer(Rate rate, AccelRange range);
    bool setGyroscope(Rate rate, GyroRange range);

    /** 새 샘플이 있으면 1. */
    int   accelerationAvailable(void);
    /** 단위 g. 성공하면 1. */
    int   readAcceleration(float &x, float &y, float &z);
    float accelerationSampleRate(void);

    int   gyroscopeAvailable(void);
    /** 단위 dps. 성공하면 1. */
    int   readGyroscope(float &x, float &y, float &z);
    float gyroscopeSampleRate(void);

    int   temperatureAvailable(void);
    /** 단위 °C. 성공하면 1. */
    int   readTemperature(float &celsius);

    /** WHO_AM_I 원시값 (0x6A 여야 한다). 응답이 없으면 -1. */
    int   whoAmI(void);

  private:
    int  statusBits(uint8_t mask);
    bool readVector(uint8_t reg, float scale, float &x, float &y, float &z);
    bool readBurst(uint8_t reg, uint8_t *buf, size_t len);
    bool writeRegister(uint8_t reg, uint8_t value);

    TwoWire &_wire;
    uint8_t  _addr;
    void    *_lock;          /* SemaphoreHandle_t */
    float    _accelScale;    /* g per LSB */
    float    _gyroScale;     /* dps per LSB */
    Rate     _accelRate;
    Rate     _gyroRate;
};

/** 보드의 IMU. Wire1(TWIM30), 0x6A. */
extern LSM6DS3TRC IMU;

#endif /* LSM6DS3TRC_H_ */
