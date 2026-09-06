/*
 * BLEService — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 * API 는 Adafruit Bluefruit52Lib 을 따른다.
 */
#ifndef _BLE_SERVICE_H_
#define _BLE_SERVICE_H_

#include "BLEUuid.h"

class BLEService
{
  public:
    BLEUuid uuid;

    BLEService(void);
    BLEService(uint16_t uuid16);
    BLEService(uint8_t const uuid128[16]);

    void setUuid(BLEUuid bleuuid);

    /** GATT 서버에 등록한다. SoftDevice 가 켜진 뒤에 불러야 한다. */
    virtual err_t begin(void);

    uint16_t handle(void) const { return _handle; }

  protected:
    uint16_t _handle;
};

#endif
