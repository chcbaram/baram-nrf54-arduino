/*
 * bluefruit.cpp — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include "sd_event_pump.h"
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

AdafruitBluefruit Bluefruit;

/* ── advertising 페이로드 ──────────────────────────────────────────── */

bool BLEAdvertisingData::addData(uint8_t type, const void *data, uint8_t len)
{
  /* AD 구조: [길이][타입][값...]. 길이는 타입까지 포함한다. */
  if (_count + len + 2 > BLE_ADV_BUF_MAX) return false;

  _buf[_count++] = len + 1;
  _buf[_count++] = type;
  memcpy(&_buf[_count], data, len);
  _count += len;
  return true;
}

bool BLEAdvertisingData::addFlags(uint8_t flags)
{
  return addData(BLE_GAP_AD_TYPE_FLAGS, &flags, 1);
}

bool BLEAdvertisingData::addTxPower(void)
{
  int8_t tx = 0;
  /* 실제 설정값을 읽어올 API 가 없어 0 dBm 으로 광고한다.
   * setTxPower() 를 쓰면 이 값과 어긋날 수 있다 — B3 에서 맞춘다. */
  return addData(BLE_GAP_AD_TYPE_TX_POWER_LEVEL, &tx, 1);
}

bool BLEAdvertisingData::addName(void)
{
  const char *name = Bluefruit.getName();
  uint8_t len = (uint8_t) strlen(name);

  /* 자리가 모자라면 잘라서 "축약된 이름" 으로 넣는다. */
  if (_count + len + 2 > BLE_ADV_BUF_MAX) {
    if (_count + 2 >= BLE_ADV_BUF_MAX) return false;
    len = BLE_ADV_BUF_MAX - _count - 2;
    return addData(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, name, len);
  }
  return addData(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, name, len);
}

bool BLEAdvertisingData::addUuid(BLEUuid bleuuid)
{
  if (bleuuid.size() == 16) {
    uint16_t u16 = bleuuid._uuid.uuid;
    return addData(BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE, &u16, 2);
  }
  if (bleuuid._uuid128 != NULL) {
    return addData(BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE, bleuuid._uuid128, 16);
  }
  return false;
}

bool BLEAdvertisingData::addService(BLEService &service)
{
  return addUuid(service.uuid);
}

/* ── advertising 제어 ──────────────────────────────────────────────── */

BLEAdvertising::BLEAdvertising(void)
{
  _handle        = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
  _fast_interval = 32;     /* 20 ms */
  _slow_interval = 244;    /* 152.5 ms */
  _fast_timeout  = 30;
  _restart       = true;
  _running       = false;
  _type          = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
  _stop_cb       = NULL;
}

void BLEAdvertising::setInterval(uint16_t fast, uint16_t slow)
{
  _fast_interval = fast;
  _slow_interval = slow;
}

void BLEAdvertising::setFastTimeout(uint16_t sec)   { _fast_timeout = sec; }
void BLEAdvertising::restartOnDisconnect(bool en)   { _restart = en; }

bool BLEAdvertising::start(uint16_t timeout)
{
  /*
   * ⚠ 광고 버퍼는 SoftDevice 가 **포인터로 들고 있는다.** 스택 변수를 넘기면
   *   광고가 도는 동안 엉뚱한 메모리를 읽는다. 그래서 _buf 는 멤버다.
   */
  ble_gap_adv_data_t data;
  memset(&data, 0, sizeof(data));
  data.adv_data.p_data      = _buf;
  data.adv_data.len         = _count;
  data.scan_rsp_data.p_data = (uint8_t *) Bluefruit.ScanResponse.buffer();
  data.scan_rsp_data.len    = Bluefruit.ScanResponse.count();

  ble_gap_adv_params_t params;
  memset(&params, 0, sizeof(params));
  params.properties.type = _type;
  params.filter_policy   = BLE_GAP_ADV_FP_ANY;
  params.interval        = _fast_interval;
  params.duration        = timeout ? (timeout * 100) : 0;   /* 10 ms 단위 */
  params.primary_phy     = BLE_GAP_PHY_1MBPS;

  uint32_t err = sd_ble_gap_adv_set_configure(&_handle, &data, &params);
  if (err != NRF_SUCCESS) return false;

  /* 우리가 설정한 연결 구성(MTU 등)을 쓰려면 그 태그로 시작해야 한다. */
  err = sd_ble_gap_adv_start(_handle, sdConnCfgTag());
  _running = (err == NRF_SUCCESS);
  return _running;
}

bool BLEAdvertising::stop(void)
{
  if (_handle == BLE_GAP_ADV_SET_HANDLE_NOT_SET) return false;
  _running = false;
  return sd_ble_gap_adv_stop(_handle) == NRF_SUCCESS;
}

void BLEAdvertising::_restartIfNeeded(void)
{
  if (_restart) start(0);
}

/* ── 싱글턴 ────────────────────────────────────────────────────────── */

AdafruitBluefruit::AdafruitBluefruit(void)
{
  strcpy(_name, "BARAM nRF54L");
  _conn_hdl    = BLE_CONN_HANDLE_INVALID;
  _cur_service = NULL;
  _char_count  = 0;
  _begun       = false;
  _tx_sem      = NULL;
  _att_mtu       = BLE_GATT_ATT_MTU_DEFAULT;
  _auto_conn_led = false;
  _conn_led_interval = 0;
  _bandwidth     = BANDWIDTH_AUTO;
  _rssi_cb       = NULL;
  memset(_chars, 0, sizeof(_chars));
}

/* sd_event_pump 가 부르는 관찰자. */
static void bluefruit_evt_observer(const ble_evt_t *evt, void *ctx)
{
  (void) ctx;
  Bluefruit._eventHandler(evt);
}

void AdafruitBluefruit::setName(const char *name)
{
  strncpy(_name, name, sizeof(_name) - 1);
  _name[sizeof(_name) - 1] = 0;

  if (_begun) {
    ble_gap_conn_sec_mode_t sec;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec);
    sd_ble_gap_device_name_set(&sec, (const uint8_t *) _name, strlen(_name));
  }
}

bool AdafruitBluefruit::begin(uint8_t prph_count, uint8_t central_count)
{
  if (_begun) return true;

  /*
   * ⚠ 지금은 **peripheral 1개 연결만** 지원한다. 세 군데가 겹친 제한이다:
   *   1. SoftDevice 설정  — sd_event_pump.c 의 SD_BLE_PERIPH_LINK_COUNT = 1
   *   2. 이 클래스        — _conn_hdl / _connection 이 배열이 아니다
   *   3. BLEUart          — 수신 FIFO 가 연결별로 없다
   *
   * 더 달라고 하면 **조용히 1개로 깎지 않고 실패시킨다.** 그래야 스케치가
   * 왜 두 번째 연결이 안 되는지 헤매지 않는다.
   */
  if (prph_count != 1 || central_count != 0) {
    return false;
  }

  if (!sdEnable()) return false;

  if (!sdBleObserverAdd(bluefruit_evt_observer, NULL)) return false;

  if (_tx_sem == NULL) {
    _tx_sem = (void *) xSemaphoreCreateBinary();
    if (_tx_sem == NULL) return false;
  }

  ble_gap_conn_sec_mode_t sec;
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec);
  if (sd_ble_gap_device_name_set(&sec, (const uint8_t *) _name, strlen(_name)) != NRF_SUCCESS) {
    return false;
  }

  _begun = true;
  return true;
}

bool AdafruitBluefruit::setTxPower(int8_t power)
{
  /* advertising 과 연결 양쪽에 건다. 광고 핸들이 아직 없으면 광고 쪽은 건너뛴다. */
  bool ok = true;
  if (Advertising.isRunning()) {
    ok &= (sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, 0, power) == NRF_SUCCESS);
  }
  if (connected()) {
    ok &= (sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, _conn_hdl, power) == NRF_SUCCESS);
  }
  return ok;
}

bool AdafruitBluefruit::disconnect(uint16_t conn_hdl)
{
  return sd_ble_gap_disconnect(conn_hdl, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) == NRF_SUCCESS;
}

bool AdafruitBluefruit::_waitTxComplete(uint32_t ms)
{
  if (_tx_sem == NULL) return false;
  return xSemaphoreTake((SemaphoreHandle_t) _tx_sem, pdMS_TO_TICKS(ms)) == pdTRUE;
}

void BLEPeriph::setConnInterval(uint16_t min, uint16_t max)
{
  _min_interval = min;
  _max_interval = max;
  _applyPpcp();
}

void BLEPeriph::setConnIntervalMS(uint16_t min_ms, uint16_t max_ms)
{
  /* 1.25 ms 단위로 바꾼다. 올림하지 않고 내림하면 규격 최소치를 밑돌 수 있어
   * (min * 4) / 5 대신 (min * 4 + 4) / 5 로 올림한다. */
  setConnInterval((uint16_t)((min_ms * 4 + 4) / 5), (uint16_t)((max_ms * 4 + 4) / 5));
}

void BLEPeriph::_applyPpcp(void)
{
  if (_min_interval == 0 || _max_interval == 0) return;

  ble_gap_conn_params_t p;
  memset(&p, 0, sizeof(p));
  p.min_conn_interval = _min_interval;
  p.max_conn_interval = _max_interval;
  p.slave_latency     = _latency;
  p.conn_sup_timeout  = _sv_timeout;

  /* SoftDevice 가 켜지기 전이면 무시된다. Bluefruit.begin() 뒤에 불러야 한다. */
  (void) sd_ble_gap_ppcp_set(&p);
}

BLEConnection *AdafruitBluefruit::Connection(uint16_t conn_hdl)
{
  if (conn_hdl == BLE_CONN_HANDLE_INVALID) return NULL;
  if (_connection.handle() != conn_hdl)    return NULL;
  return &_connection;
}

void AdafruitBluefruit::autoConnLed(bool enable)
{
  _auto_conn_led = enable;
#ifdef LED_CONN
  if (enable) {
    pinMode(LED_CONN, OUTPUT);
    if (connected()) ledOn(LED_CONN); else ledOff(LED_CONN);
  }
#endif
}

void AdafruitBluefruit::configPrphConn(uint16_t mtu, uint8_t event_len,
                                       uint8_t hvn_qsize, uint8_t wrcmd_qsize)
{
  /*
   * Adafruit 시그니처 호환용. 이 값들은 sd_ble_cfg_set() 으로 **sd_ble_enable()
   * 전에** 정해져야 하는데, 그 시점은 sdEnable() 안이고 여기보다 앞이다.
   * 지금은 sd_event_pump.c 의 컴파일 타임 설정(SD_BLE_ATT_MTU 등)을 쓴다.
   * 런타임으로 바꾸려면 sdEnable() 을 파라미터화해야 한다.
   */
  (void) mtu; (void) event_len; (void) hvn_qsize; (void) wrcmd_qsize;
}

bool AdafruitBluefruit::_registerChar(BLECharacteristic *chr)
{
  if (_char_count >= BLE_MAX_CHARS) return false;
  _chars[_char_count++] = chr;
  return true;
}

void AdafruitBluefruit::_eventHandler(const ble_evt_t *evt)
{
  switch (evt->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED:
      _conn_hdl = evt->evt.gap_evt.conn_handle;
      _connection._begin(evt);
      Advertising._setRunning(false);
#ifdef LED_CONN
      if (_auto_conn_led) ledOn(LED_CONN);
#endif
      if (Periph._connect_cb) Periph._connect_cb(_conn_hdl);
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      if (Periph._disconnect_cb) {
        Periph._disconnect_cb(evt->evt.gap_evt.conn_handle,
                              evt->evt.gap_evt.params.disconnected.reason);
      }
      _conn_hdl = BLE_CONN_HANDLE_INVALID;
      _att_mtu  = BLE_GATT_ATT_MTU_DEFAULT;   /* 다음 연결에서 다시 협상한다 */
      _connection._end();
#ifdef LED_CONN
      if (_auto_conn_led) ledOff(LED_CONN);
#endif
      Advertising._restartIfNeeded();
      break;

    /*
     * ⚠ 이 두 가지에 응답하지 않으면 연결이 조용히 끊긴다.
     *   SoftDevice 가 앱의 답을 기다리다 타임아웃을 낸다.
     */
    case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
      ble_gap_phys_t phys = { BLE_GAP_PHY_AUTO, BLE_GAP_PHY_AUTO };
      sd_ble_gap_phy_update(evt->evt.gap_evt.conn_handle, &phys);
      break;
    }

    /*
     * ⚠ 데이터 길이 갱신 요청에 답하지 않으면 **연결은 유지되는데 ATT 가 전혀
     *   흐르지 않는다.** 링크 계층 절차가 끝나지 않아 호스트의 서비스 탐색이
     *   시작조차 못 하고, 결국 호스트가 타임아웃으로 끊는다(reason 0x13).
     *   실제로 이 증상을 겪었다: 이벤트가 0x10(연결) → 0x21 → 0x22 → 0x23 에서
     *   멈추고 GATTS 이벤트가 하나도 오지 않았다.
     *   NULL 을 넘기면 SoftDevice 가 알아서 최대치를 고른다.
     */
    case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
      sd_ble_gap_data_length_update(evt->evt.gap_evt.conn_handle, NULL, NULL);
      break;

    /* 연결 파라미터 갱신 요청은 상대가 제안한 값을 그대로 받는다. */
    case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
      sd_ble_gap_conn_param_update(
          evt->evt.gap_evt.conn_handle,
          &evt->evt.gap_evt.params.conn_param_update_request.conn_params);
      break;

    /*
     * ⚠ 여기서 BLE_GATT_ATT_MTU_DEFAULT 를 하드코딩하면 안 된다.
     *   스택을 큰 MTU 로 구성해 놓고도 상대와는 23 으로 협상돼, 큰 쓰기가
     *   전부 ATT long write 로 가고 그 경로가 없어 연결이 끊긴다. 실제로 겪었다.
     *   실효 MTU 는 양쪽 제시값 중 작은 쪽이다.
     */
    case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST: {
      uint16_t ours   = sdAttMtu();
      uint16_t theirs = evt->evt.gatts_evt.params.exchange_mtu_request.client_rx_mtu;

      sd_ble_gatts_exchange_mtu_reply(evt->evt.gatts_evt.conn_handle, ours);

      if (theirs < BLE_GATT_ATT_MTU_DEFAULT) theirs = BLE_GATT_ATT_MTU_DEFAULT;
      _att_mtu = (theirs < ours) ? theirs : ours;
      _connection._setMtu(_att_mtu);
      break;
    }

    /* notify 한 건이 무선으로 나갔다. 기다리던 쪽을 깨운다. */
    case BLE_GAP_EVT_ADV_SET_TERMINATED:
      Advertising._onStopped();
      break;

    case BLE_GAP_EVT_RSSI_CHANGED:
      _connection._setRssi(evt->evt.gap_evt.params.rssi_changed.rssi);
      if (_rssi_cb) _rssi_cb(evt->evt.gap_evt.conn_handle,
                             evt->evt.gap_evt.params.rssi_changed.rssi);
      break;

    case BLE_GATTS_EVT_HVN_TX_COMPLETE:
      if (_tx_sem) xSemaphoreGive((SemaphoreHandle_t) _tx_sem);
      break;

    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
      /* 본딩을 안 하므로 시스템 속성이 없다. NULL 로 답한다. */
      sd_ble_gatts_sys_attr_set(evt->evt.gatts_evt.conn_handle, NULL, 0, 0);
      break;

    default:
      break;
  }

  /* characteristic 쓰기 이벤트 전달 */
  for (uint8_t i = 0; i < _char_count; i++) {
    if (_chars[i]) _chars[i]->_eventHandler(evt);
  }
}
