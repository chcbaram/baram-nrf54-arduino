/*
 * BLEUuid — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 adafruit/Adafruit_nRF52_Arduino 의 Bluefruit52Lib/src/BLEUuid.h 를
 * 따른다 (MIT/BSD). 구현은 S145 기준으로 새로 썼다.
 */
#ifndef _BLE_UUID_H_
#define _BLE_UUID_H_

#include <Arduino.h>
#include <ble.h>

class BLEUuid
{
  public:
    ble_uuid_t      _uuid;
    uint8_t const  *_uuid128;

    BLEUuid(void);
    BLEUuid(uint16_t uuid16);
    BLEUuid(uint8_t const uuid128[16]);
    BLEUuid(ble_uuid_t uuid);

    void set(uint16_t uuid16);
    void set(uint8_t const uuid128[16]);
    void set(ble_uuid_t uuid);

    bool   get(uint16_t *uuid16) const;
    size_t size(void) const;          /* 비트 수: 16 또는 128 */

    /*
     * 128비트 UUID 를 SoftDevice 에 등록해 type 을 받아 온다.
     * 16비트면 할 일이 없다.
     *
     * ⚠ 반드시 SoftDevice 가 켜진 뒤에 불러야 한다. sd_ble_uuid_vs_add() 는
     *   SVC 라서 SD 가 없으면 널 포인터 포워딩이 된다 (sd_svc_dispatch.S).
     */
    bool begin(void);

    bool operator==(const BLEUuid   &uuid) const;
    bool operator!=(const BLEUuid   &uuid) const;
    bool operator==(const ble_uuid_t uuid) const;
    bool operator!=(const ble_uuid_t uuid) const;

    BLEUuid& operator=(const uint16_t uuid);
    BLEUuid& operator=(uint8_t const uuid128[16]);
    BLEUuid& operator=(ble_uuid_t uuid);
};

#endif
