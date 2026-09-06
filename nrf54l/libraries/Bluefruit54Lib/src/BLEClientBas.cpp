/*
 * BLEClientBas / BLEClientDis — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"

/* ── 배터리 ───────────────────────────────────────────────────────── */

BLEClientBas::BLEClientBas(void)
  : BLEClientService(BLEUuid(UUID16_SVC_BATTERY_SERVICE)),
    _battery(BLEUuid(UUID16_CHR_BATTERY_LEVEL))
{
}

bool BLEClientBas::begin(void)
{
  if (!BLEClientService::begin()) return false;
  _battery.begin(this);
  return true;
}

bool BLEClientBas::discover(uint16_t conn_hdl)
{
  if (!BLEClientService::discover(conn_hdl)) return false;

  discoverCharacteristics();
  if (!_battery.discovered()) {
    _disconnected();
    return false;
  }
  return true;
}

uint8_t BLEClientBas::read(void)
{
  return _battery.read8();
}

/* ── 장치 정보 ────────────────────────────────────────────────────── */

BLEClientDis::BLEClientDis(void)
  : BLEClientService(BLEUuid(UUID16_SVC_DEVICE_INFORMATION))
{
}

bool BLEClientDis::begin(void)
{
  return BLEClientService::begin();
}

bool BLEClientDis::discover(uint16_t conn_hdl)
{
  /* 서비스 범위만 찾아 두면 된다. characteristic 은 getChars() 가 그때 찾는다. */
  return BLEClientService::discover(conn_hdl);
}

uint16_t BLEClientDis::getChars(uint16_t uuid16, char *buffer, uint16_t bufsize)
{
  if (buffer == NULL || bufsize == 0) return 0;
  buffer[0] = 0;
  if (!discovered()) return 0;

  /*
   * UUID 로 직접 읽는다. 핸들을 몰라도 SoftDevice 가 탐색과 읽기를 한 번에
   * 하므로, 서비스 범위 안에서만 찾게 하면 이게 가장 짧다.
   */
  uint16_t len = Bluefruit.Gatt.readCharByUuid(connHandle(), BLEUuid(uuid16),
                                               buffer, (uint16_t) (bufsize - 1),
                                               _hdl_range.start_handle,
                                               _hdl_range.end_handle);
  buffer[len] = 0;
  return len;
}
