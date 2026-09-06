/*
 * BLEService — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "BLEService.h"
#include "bluefruit.h"

BLEService::BLEService(void)                       : uuid()        { _handle = BLE_GATT_HANDLE_INVALID; }
BLEService::BLEService(uint16_t uuid16)            : uuid(uuid16)  { _handle = BLE_GATT_HANDLE_INVALID; }
BLEService::BLEService(uint8_t const uuid128[16])  : uuid(uuid128) { _handle = BLE_GATT_HANDLE_INVALID; }

void BLEService::setUuid(BLEUuid bleuuid)
{
  uuid = bleuuid;
}

err_t BLEService::begin(void)
{
  if (!uuid.begin()) {
    return NRF_ERROR_INVALID_PARAM;
  }

  /*
   * 서비스를 등록하면 그 뒤에 추가되는 characteristic 은 이 서비스에 붙는다.
   * SoftDevice 의 GATT 서버가 "마지막에 등록된 서비스" 를 기억하는 구조라
   * **등록 순서가 곧 소속**이다. Bluefruit 도 같은 전제로 동작하므로
   * service.begin() 다음에 그 서비스의 characteristic.begin() 을 불러야 한다.
   */
  Bluefruit._setCurrentService(this);

  return sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &uuid._uuid, &_handle);
}
