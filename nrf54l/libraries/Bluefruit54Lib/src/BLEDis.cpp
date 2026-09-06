/*
 * BLEDis — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "BLEDis.h"
#include <string.h>

/* Device Information Service 와 그 characteristic 들의 표준 16비트 UUID */
#define UUID16_SVC_DEVICE_INFORMATION   0x180A
#define UUID16_CHR_MANUFACTURER_NAME    0x2A29
#define UUID16_CHR_MODEL_NUMBER         0x2A24
#define UUID16_CHR_SERIAL_NUMBER        0x2A25
#define UUID16_CHR_HARDWARE_REVISION    0x2A27
#define UUID16_CHR_FIRMWARE_REVISION    0x2A26
#define UUID16_CHR_SOFTWARE_REVISION    0x2A28

BLEDis::BLEDis(void)
  : BLEService(UUID16_SVC_DEVICE_INFORMATION)
{
  _mfr = _model = _serial = _hw = _fw = _sw = NULL;
}

err_t BLEDis::begin(void)
{
  err_t err = BLEService::begin();
  if (err) return err;

  const char *values[BLE_DIS_CHAR_COUNT] = { _mfr, _model, _serial, _hw, _fw, _sw };
  const uint16_t uuids[BLE_DIS_CHAR_COUNT] = {
    UUID16_CHR_MANUFACTURER_NAME, UUID16_CHR_MODEL_NUMBER,
    UUID16_CHR_SERIAL_NUMBER,     UUID16_CHR_HARDWARE_REVISION,
    UUID16_CHR_FIRMWARE_REVISION, UUID16_CHR_SOFTWARE_REVISION,
  };

  for (uint8_t i = 0; i < BLE_DIS_CHAR_COUNT; i++) {
    /* 설정하지 않은 항목은 아예 만들지 않는다. 빈 characteristic 을 노출하면
     * 상대가 읽었을 때 의미 없는 빈 문자열이 나가고 속성 테이블만 먹는다. */
    if (values[i] == NULL) continue;

    uint16_t len = (uint16_t) strlen(values[i]);

    _chars[i].setUuid(BLEUuid(uuids[i]));
    _chars[i].setProperties(CHR_PROPS_READ);
    _chars[i].setFixedLen(len);
    err = _chars[i].begin();
    if (err) return err;

    _chars[i].write(values[i], len);
  }

  return NRF_SUCCESS;
}
