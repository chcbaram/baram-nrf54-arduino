/*
 * BLEBas — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "BLEBas.h"

#define UUID16_SVC_BATTERY_SERVICE   0x180F
#define UUID16_CHR_BATTERY_LEVEL     0x2A19

BLEBas::BLEBas(void)
  : BLEService(UUID16_SVC_BATTERY_SERVICE), _level_chr(UUID16_CHR_BATTERY_LEVEL)
{
  _level = 100;
}

err_t BLEBas::begin(void)
{
  err_t err = BLEService::begin();
  if (err) return err;

  _level_chr.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  _level_chr.setFixedLen(1);
  err = _level_chr.begin();
  if (err) return err;

  return (_level_chr.write(&_level, 1) == 1) ? NRF_SUCCESS : NRF_ERROR_INVALID_STATE;
}

bool BLEBas::write(uint8_t percent)
{
  if (percent > 100) percent = 100;
  _level = percent;

  /* 알림이 켜져 있으면 notify() 가 값 갱신까지 한다.
   * 안 켜져 있으면 값만 갱신된다 (BLECharacteristic::notify 주석 참조). */
  _level_chr.notify(&_level, 1);
  return true;
}
