/*
 * LSM6DS3TRC.cpp — XIAO nRF54LM20A Sense 의 IMU (ST LSM6DS3TR-C)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "LSM6DS3TRC.h"
#include "XIAO_nRF54LM20A.h"

#include "FreeRTOS.h"
#include "semphr.h"

/* ── 레지스터 (ST lsm6ds3tr-c_reg.h) ─────────────────────────────────── */
#define REG_WHO_AM_I      0x0F
#define REG_CTRL1_XL      0x10    /* odr_xl[7:4] fs_xl[3:2] */
#define REG_CTRL2_G       0x11    /* odr_g[7:4]  fs_g[3:1]  */
#define REG_CTRL3_C       0x12
#define REG_STATUS        0x1E
#define REG_OUT_TEMP_L    0x20
#define REG_OUTX_L_G      0x22
#define REG_OUTX_L_XL     0x28

#define WHO_AM_I_VALUE    0x6A

#define CTRL3_C_IF_INC    0x04    /* 연속 읽기 때 주소 자동 증가 (리셋 기본값) */
#define CTRL3_C_BDU       0x40    /* 상·하위 바이트를 한 샘플에서 읽게 보장 */

#define STATUS_XLDA       0x01
#define STATUS_GDA        0x02
#define STATUS_TDA        0x04

/* 레일이 올라오고 IMU 가 부팅할 시간. 데이터시트의 boot time(15 ms)에 여유를 둔다. */
#define POWER_UP_MS       50

LSM6DS3TRC IMU(Wire1, IMU_I2C_ADDRESS);

LSM6DS3TRC::LSM6DS3TRC(TwoWire &wire, uint8_t address)
  : _wire(wire), _addr(address), _lock(NULL),
    _accelScale(0), _gyroScale(0), _accelRate(RATE_OFF), _gyroRate(RATE_OFF)
{
  /* 생성자에서는 하드웨어를 건드리지 않는다. 전역 객체라 main() 전에 불린다. */
}

int LSM6DS3TRC::begin(void)
{
  if (_lock == NULL) {
    _lock = xSemaphoreCreateMutex();          /* 초기화 때 한 번 */
    if (_lock == NULL) return 0;
  }

  if (!PMIC.begin() || !PMIC.setSensorPower(true)) return 0;
  delay(POWER_UP_MS);

  _wire.begin();
  if (whoAmI() != WHO_AM_I_VALUE) return 0;

  if (!writeRegister(REG_CTRL3_C, CTRL3_C_IF_INC | CTRL3_C_BDU)) return 0;
  if (!setAccelerometer(RATE_104HZ, ACCEL_4G))      return 0;
  if (!setGyroscope(RATE_104HZ, GYRO_2000DPS))      return 0;
  return 1;
}

void LSM6DS3TRC::end(void)
{
  setAccelerometer(RATE_OFF, ACCEL_4G);
  setGyroscope(RATE_OFF, GYRO_2000DPS);
}

bool LSM6DS3TRC::setAccelerometer(Rate rate, AccelRange range)
{
  float mg = 0;

  switch (range) {
    case ACCEL_2G:
      mg = 0.061f;
      break;

    case ACCEL_4G:
      mg = 0.122f;
      break;

    case ACCEL_8G:
      mg = 0.244f;
      break;

    case ACCEL_16G:
      mg = 0.488f;
      break;
  }
  if (mg == 0 || rate > RATE_6660HZ) return false;

  if (!writeRegister(REG_CTRL1_XL, (uint8_t) ((rate << 4) | (range << 2)))) return false;
  _accelScale = mg / 1000.0f;
  _accelRate  = rate;
  return true;
}

bool LSM6DS3TRC::setGyroscope(Rate rate, GyroRange range)
{
  float mdps = 0;

  switch (range) {
    case GYRO_125DPS:
      mdps = 4.375f;
      break;

    case GYRO_250DPS:
      mdps = 8.75f;
      break;

    case GYRO_500DPS:
      mdps = 17.5f;
      break;

    case GYRO_1000DPS:
      mdps = 35.0f;
      break;

    case GYRO_2000DPS:
      mdps = 70.0f;
      break;
  }
  /* 자이로는 6.66 kHz 가 없다 (ST GY_ODR 표는 3k33 까지). */
  if (mdps == 0 || rate > RATE_3330HZ) return false;

  if (!writeRegister(REG_CTRL2_G, (uint8_t) ((rate << 4) | (range << 1)))) return false;
  _gyroScale = mdps / 1000.0f;
  _gyroRate  = rate;
  return true;
}

int LSM6DS3TRC::accelerationAvailable(void) { return statusBits(STATUS_XLDA); }
int LSM6DS3TRC::gyroscopeAvailable(void)    { return statusBits(STATUS_GDA); }
int LSM6DS3TRC::temperatureAvailable(void)  { return statusBits(STATUS_TDA); }

int LSM6DS3TRC::readAcceleration(float &x, float &y, float &z)
{
  return readVector(REG_OUTX_L_XL, _accelScale, x, y, z) ? 1 : 0;
}

int LSM6DS3TRC::readGyroscope(float &x, float &y, float &z)
{
  return readVector(REG_OUTX_L_G, _gyroScale, x, y, z) ? 1 : 0;
}

int LSM6DS3TRC::readTemperature(float &celsius)
{
  uint8_t b[2];
  if (!readBurst(REG_OUT_TEMP_L, b, sizeof(b))) return 0;

  int16_t raw = (int16_t) (b[1] << 8 | b[0]);
  celsius = raw / 256.0f + 25.0f;             /* ST lsm6ds3tr_c_from_lsb_to_celsius */
  return 1;
}

static float rateHz(uint8_t rate)
{
  static const float hz[] = { 0, 12.5f, 26, 52, 104, 208, 416, 833, 1660, 3330, 6660 };
  return (rate < sizeof(hz) / sizeof(hz[0])) ? hz[rate] : 0;
}

float LSM6DS3TRC::accelerationSampleRate(void) { return rateHz(_accelRate); }
float LSM6DS3TRC::gyroscopeSampleRate(void)    { return rateHz(_gyroRate); }

int LSM6DS3TRC::whoAmI(void)
{
  uint8_t v;
  return readBurst(REG_WHO_AM_I, &v, 1) ? v : -1;
}

/*───────────────────────────────────────────────────────────────────*/

int LSM6DS3TRC::statusBits(uint8_t mask)
{
  uint8_t st;
  return (readBurst(REG_STATUS, &st, 1) && (st & mask)) ? 1 : 0;
}

bool LSM6DS3TRC::readVector(uint8_t reg, float scale, float &x, float &y, float &z)
{
  uint8_t b[6];
  if (scale == 0 || !readBurst(reg, b, sizeof(b))) return false;

  x = (int16_t) (b[1] << 8 | b[0]) * scale;
  y = (int16_t) (b[3] << 8 | b[2]) * scale;
  z = (int16_t) (b[5] << 8 | b[4]) * scale;
  return true;
}

bool LSM6DS3TRC::readBurst(uint8_t reg, uint8_t *buf, size_t len)
{
  if (_lock == NULL) return false;

  bool ok = false;

  /* "주소 쓰기 → 반복 시작 → 읽기" 가 한 트랜잭션이다. 사이에 이 클래스의 다른
   * 호출이 끼지 않게 전체를 묶는다. */
  xSemaphoreTake((SemaphoreHandle_t) _lock, portMAX_DELAY);
  _wire.beginTransmission(_addr);
  _wire.write(reg);
  if (_wire.endTransmission(false) == 0 && _wire.requestFrom(_addr, len) == len) {
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t) _wire.read();
    ok = true;
  }
  xSemaphoreGive((SemaphoreHandle_t) _lock);

  return ok;
}

bool LSM6DS3TRC::writeRegister(uint8_t reg, uint8_t value)
{
  if (_lock == NULL) return false;

  xSemaphoreTake((SemaphoreHandle_t) _lock, portMAX_DELAY);
  _wire.beginTransmission(_addr);
  _wire.write(reg);
  _wire.write(value);
  bool ok = (_wire.endTransmission() == 0);
  xSemaphoreGive((SemaphoreHandle_t) _lock);

  return ok;
}
