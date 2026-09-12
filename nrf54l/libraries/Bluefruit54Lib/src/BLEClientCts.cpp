/*
 * BLEClientCts — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

static_assert(sizeof(((BLEClientCts *) 0)->Time) == 10, "Current Time is 10 bytes");
static_assert(sizeof(((BLEClientCts *) 0)->LocalInfo) == 2, "Local Time Info is 2 bytes");

/*
 * 알림은 정적 함수로 들어온다. 상류처럼 characteristic 에서 서비스를 거슬러
 * 올라간다 — 클라이언트 쪽에는 parentService() 가 있다.
 */
static void blects_notify_cb(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
{
  BLEClientCts *cts = (BLEClientCts *) chr->parentService();
  if (cts) cts->_curTimeNotify(data, len);
}

BLEClientCts::BLEClientCts(void)
  : BLEClientService(BLEUuid(UUID16_SVC_CURRENT_TIME)),
    _cur_time(BLEUuid(UUID16_CHR_CURRENT_TIME)),
    _local_info(BLEUuid(UUID16_CHR_LOCAL_TIME_INFORMATION))
{
  memset(&Time, 0, sizeof(Time));
  memset(&LocalInfo, 0, sizeof(LocalInfo));
  _adjust_cb = NULL;
}

bool BLEClientCts::begin(void)
{
  if (!BLEClientService::begin()) return false;

  _cur_time.begin(this);
  _cur_time.setNotifyCallback(blects_notify_cb);

  _local_info.begin(this);
  return true;
}

bool BLEClientCts::discover(uint16_t conn_hdl)
{
  if (!BLEClientService::discover(conn_hdl)) return false;

  discoverCharacteristics();

  /*
   * Current Time 만 필수다. Local Time Information 은 규격상 선택이라
   * **없다고 실패시키면 안 된다** — 있는 상대와 없는 상대가 둘 다 흔하다.
   */
  if (!_cur_time.discovered()) {
    _disconnected();
    return false;
  }
  return true;
}

bool BLEClientCts::getCurrentTime(void)
{
  return _cur_time.read(&Time, sizeof(Time)) > 0;
}

bool BLEClientCts::getLocalTimeInfo(void)
{
  if (!_local_info.discovered()) return false;
  return _local_info.read(&LocalInfo, sizeof(LocalInfo)) > 0;
}

bool BLEClientCts::enableAdjust(void)
{
  return _cur_time.enableNotify();
}

void BLEClientCts::setAdjustCallback(adjust_callback_t fp)
{
  _adjust_cb = fp;
}

void BLEClientCts::_curTimeNotify(uint8_t *data, uint16_t len)
{
  /*
   * ⚠ 길이를 자른다. 상류는 받은 길이를 그대로 memcpy 해서, 상대가 규격보다
   *   긴 값을 보내면 구조체 뒤를 넘어 쓴다. 짧게 오는 경우도 있으므로 둘 다 본다.
   */
  if (len > sizeof(Time)) len = sizeof(Time);
  memcpy(&Time, data, len);

  if (_adjust_cb) _adjust_cb(Time.adjust_reason);
}
