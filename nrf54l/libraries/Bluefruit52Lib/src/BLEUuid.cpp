/*
 * BLEUuid — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "BLEUuid.h"
#include <string.h>

BLEUuid::BLEUuid(void)
{
  _uuid.type = BLE_UUID_TYPE_UNKNOWN;
  _uuid.uuid = 0;
  _uuid128   = NULL;
}

BLEUuid::BLEUuid(uint16_t uuid16)            { _uuid128 = NULL; set(uuid16);  }
BLEUuid::BLEUuid(uint8_t const uuid128[16])  { set(uuid128);                  }
BLEUuid::BLEUuid(ble_uuid_t uuid)            { _uuid128 = NULL; set(uuid);    }

void BLEUuid::set(uint16_t uuid16)
{
  _uuid.type = BLE_UUID_TYPE_BLE;
  _uuid.uuid = uuid16;
  _uuid128   = NULL;
}

void BLEUuid::set(uint8_t const uuid128[16])
{
  /*
   * SoftDevice 의 128비트 UUID 는 "베이스 + 16비트" 로 다룬다.
   * 바이트 12~13 이 그 16비트 자리다 (little-endian 배열이므로
   * uuid128[12] 가 하위, [13] 이 상위).
   */
  _uuid128   = uuid128;
  _uuid.type = BLE_UUID_TYPE_UNKNOWN;   /* begin() 에서 채워진다 */
  _uuid.uuid = (uint16_t) ((uuid128[13] << 8) | uuid128[12]);
}

void BLEUuid::set(ble_uuid_t uuid)
{
  _uuid    = uuid;
  _uuid128 = NULL;
}

bool BLEUuid::get(uint16_t *uuid16) const
{
  if (uuid16 == NULL) return false;
  *uuid16 = _uuid.uuid;
  return true;
}

size_t BLEUuid::size(void) const
{
  return (_uuid.type == BLE_UUID_TYPE_BLE) ? 16 : 128;
}

bool BLEUuid::begin(void)
{
  if (_uuid128 == NULL) {
    /* 16비트는 등록할 것이 없다. */
    return true;
  }

  /*
   * 베이스 UUID 를 등록한다. 12~13 바이트를 0 으로 비운 것이 "베이스" 다.
   * SoftDevice 가 type 을 돌려주고, 이후 그 type + 16비트로 참조한다.
   */
  ble_uuid128_t base;
  memcpy(base.uuid128, _uuid128, 16);
  base.uuid128[12] = 0;
  base.uuid128[13] = 0;

  uint8_t  type = BLE_UUID_TYPE_UNKNOWN;
  uint32_t err  = sd_ble_uuid_vs_add(&base, &type);
  if (err != NRF_SUCCESS) {
    return false;
  }

  _uuid.type = type;
  return true;
}

bool BLEUuid::operator==(const BLEUuid &uuid) const
{
  return (_uuid.type == uuid._uuid.type) && (_uuid.uuid == uuid._uuid.uuid);
}
bool BLEUuid::operator!=(const BLEUuid &uuid) const   { return !(*this == uuid); }
bool BLEUuid::operator==(const ble_uuid_t uuid) const
{
  return (_uuid.type == uuid.type) && (_uuid.uuid == uuid.uuid);
}
bool BLEUuid::operator!=(const ble_uuid_t uuid) const { return !(*this == uuid); }

BLEUuid& BLEUuid::operator=(const uint16_t uuid)          { set(uuid);    return *this; }
BLEUuid& BLEUuid::operator=(uint8_t const uuid128[16])    { set(uuid128); return *this; }
BLEUuid& BLEUuid::operator=(ble_uuid_t uuid)              { set(uuid);    return *this; }
