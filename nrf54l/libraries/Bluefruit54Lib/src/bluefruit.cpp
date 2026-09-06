/*
 * bluefruit.cpp — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include "sd_event_pump.h"
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

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

bool BLEAdvertising::setBeacon(BLEBeacon &beacon)
{
  return beacon.start(*this);
}

bool BLEAdvertising::setBeacon(EddyStoneUrl &eddy_url)
{
  return eddy_url.start();
}

void BLEAdvertising::_restartIfNeeded(void)
{
  /*
   * 이미 돌고 있으면 건드리지 않는다. 다중 연결에서는 연결된 채로 광고를
   * 계속 켜 두는 게 정상이라, 그 상태에서 다른 링크가 끊기면 여기로 들어온다.
   * 그때 다시 start() 하면 SoftDevice 가 INVALID_STATE 를 내고 _running 이
   * false 로 뒤집혀, **광고는 도는데 라이브러리는 안 돈다고 아는** 상태가 된다.
   */
  if (_restart && !_running) start(0);
}

/* ── 스케치 콜백 지연 실행 ─────────────────────────────────────────── */

/*
 * ⚠ 스케치 콜백을 BLE 이벤트 태스크에서 직접 부르면 안 된다.
 *
 *   상류 예제는 연결 콜백 안에서 getPeerName() — 블로킹 GATT 읽기 — 을 부른다.
 *   그걸 이벤트 태스크에서 하면 기다리는 응답 이벤트를 처리할 주체가 자기
 *   자신이라 타임아웃까지 멈춘다. Adafruit 이 ada_callback() 으로 콜백을 다른
 *   태스크에 넘기는 이유가 그것이다. 우리도 같은 구조를 쓴다.
 *
 *   부수 효과로 콜백이 오래 걸려도 이벤트 펌프가 막히지 않는다.
 */
enum { BLE_CB_CONNECT = 0, BLE_CB_DISCONNECT };

typedef struct {
  uint8_t  type;
  uint8_t  reason;
  uint16_t conn_hdl;
} ble_cb_msg_t;

/* 링크마다 연결+해제가 겹칠 수 있으므로 넉넉히 잡는다. */
#define BLE_CB_QUEUE_LEN      (BLE_MAX_CONNECTION * 4)
#define BLE_CB_TASK_STACK     (512)
/* 이벤트 태스크(3)보다 낮게 둔다 — 펌프가 항상 먼저 돌아야 한다. */
#define BLE_CB_TASK_PRIORITY  (2)

static void ble_cb_task(void *arg)
{
  (void) arg;
  Bluefruit._callbackTask();
}

void AdafruitBluefruit::_callbackTask(void)
{
  ble_cb_msg_t msg;

  while (1) {
    if (xQueueReceive((QueueHandle_t) _cb_queue, &msg, portMAX_DELAY) != pdTRUE) continue;

    switch (msg.type) {
      case BLE_CB_CONNECT:
        if (Periph._connect_cb) Periph._connect_cb(msg.conn_hdl);
        break;

      case BLE_CB_DISCONNECT: {
        if (Periph._disconnect_cb) Periph._disconnect_cb(msg.conn_hdl, msg.reason);

        /*
         * ⚠ 슬롯 반납은 **콜백이 끝난 뒤**다. 이벤트 핸들러에서 바로 반납하면
         *   콜백이 도는 시점엔 이미 남의 연결이 그 슬롯에 들어와 있을 수 있다.
         */
        int8_t slot = _slotOf(msg.conn_hdl);
        if (slot >= 0) _connection[slot]._end();
        break;
      }

      default:
        break;
    }
  }
}

void AdafruitBluefruit::_deferConnect(uint16_t conn_hdl)
{
  ble_cb_msg_t msg = { BLE_CB_CONNECT, 0, conn_hdl };
  if (_cb_queue) xQueueSend((QueueHandle_t) _cb_queue, &msg, 0);
}

void AdafruitBluefruit::_deferDisconnect(uint16_t conn_hdl, uint8_t reason)
{
  ble_cb_msg_t msg = { BLE_CB_DISCONNECT, reason, conn_hdl };
  if (_cb_queue) xQueueSend((QueueHandle_t) _cb_queue, &msg, 0);
}

/* ── 싱글턴 ────────────────────────────────────────────────────────── */

AdafruitBluefruit::AdafruitBluefruit(void)
{
  strcpy(_name, "BARAM nRF54L");
  _conn_hdl    = BLE_CONN_HANDLE_INVALID;
  _prph_count    = 1;
  _central_count = 0;
  _cur_service = NULL;
  _char_count  = 0;
  _begun       = false;
  _auto_conn_led = false;
  _conn_led_interval = 0;
  _bandwidth     = BANDWIDTH_AUTO;
  _rssi_cb       = NULL;
  _cb_queue    = NULL;
  _cb_task     = NULL;
  memset(_chars, 0, sizeof(_chars));
  memset(_tx_sem, 0, sizeof(_tx_sem));
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
   * ⚠ 링크 수는 **SoftDevice RAM 예약과 한 몸이라 컴파일 타임에 정해진다**
   *   (boards.txt 의 -DSD_BLE_PERIPH_LINK_COUNT). 런타임에 늘릴 수 없다.
   *   요청이 그보다 크면 **조용히 깎지 않고 실패시킨다** — 그래야 스케치가
   *   왜 N번째 연결이 안 되는지 헤매지 않는다.
   *
   * 역할별 상한도 따로다. peripheral 과 central 은 버퍼를 공유하지만
   * (S145 는 연결 구성이 하나뿐이다) 역할 수는 role_count 로 따로 정해진다.
   */
  if (prph_count > BLE_MAX_PERIPH_CONNECTION)     return false;
  if (central_count > BLE_MAX_CENTRAL_CONNECTION) return false;
  if (prph_count == 0 && central_count == 0)      return false;

  _prph_count    = prph_count;
  _central_count = central_count;

  if (!sdEnable()) return false;

  if (!sdBleObserverAdd(bluefruit_evt_observer, NULL)) return false;

  for (uint8_t i = 0; i < BLE_MAX_CONNECTION; i++) {
    if (_tx_sem[i] == NULL) {
      _tx_sem[i] = (void *) xSemaphoreCreateBinary();
      if (_tx_sem[i] == NULL) return false;
    }
  }

  if (_cb_queue == NULL) {
    _cb_queue = (void *) xQueueCreate(BLE_CB_QUEUE_LEN, sizeof(ble_cb_msg_t));
    if (_cb_queue == NULL) return false;
  }
  if (_cb_task == NULL) {
    if (xTaskCreate(ble_cb_task, "ble_cb", BLE_CB_TASK_STACK, NULL,
                    BLE_CB_TASK_PRIORITY, (TaskHandle_t *) &_cb_task) != pdPASS) {
      return false;
    }
  }

  if (!Gatt._begin()) return false;

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
  /* 연결별로 걸어야 한다 — 역할이 아니라 링크 단위 설정이다. */
  for (uint8_t i = 0; i < BLE_MAX_CONNECTION; i++) {
    uint16_t h = connHandleAt(i);
    if (h == BLE_CONN_HANDLE_INVALID) continue;
    ok &= (sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, h, power) == NRF_SUCCESS);
  }
  return ok;
}

bool AdafruitBluefruit::disconnect(uint16_t conn_hdl)
{
  return sd_ble_gap_disconnect(conn_hdl, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) == NRF_SUCCESS;
}

bool AdafruitBluefruit::_waitTxComplete(uint16_t conn_hdl, uint32_t ms)
{
  int8_t slot = _slotOf(conn_hdl);
  if (slot < 0)             return false;
  if (_tx_sem[slot] == NULL) return false;
  return xSemaphoreTake((SemaphoreHandle_t) _tx_sem[slot], pdMS_TO_TICKS(ms)) == pdTRUE;
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

/*
 * ⚠ 연결 핸들은 슬롯 번호가 **아니다.** 링크 수보다 큰 핸들이 온다 — 링크 수를
 *   4 로 두고도 SoftDevice 가 핸들 4 를 주는 것을 실기에서 봤다. 그래서 핸들로
 *   배열을 찍지 않고 슬롯을 뒤진다. 슬롯이 몇 개 안 되므로 선형 탐색으로 충분하다.
 */
int8_t AdafruitBluefruit::_slotOf(uint16_t conn_hdl) const
{
  if (conn_hdl == BLE_CONN_HANDLE_INVALID) return -1;

  for (uint8_t i = 0; i < BLE_MAX_CONNECTION; i++) {
    if (_connection[i]._inUse() && _connection[i].handle() == conn_hdl) return (int8_t) i;
  }
  return -1;
}

BLEConnection *AdafruitBluefruit::Connection(uint16_t conn_hdl)
{
  /*
   * 끊긴 직후 disconnect 콜백 동안에도 준다 — 스케치가 peer 주소 같은 마지막
   * 정보를 읽을 수 있어야 한다. 연결 여부는 connected() 로 따로 본다.
   */
  int8_t slot = _slotOf(conn_hdl);
  return (slot < 0) ? NULL : &_connection[slot];
}

uint16_t AdafruitBluefruit::connHandleAt(uint8_t idx) const
{
  if (idx >= BLE_MAX_CONNECTION)   return BLE_CONN_HANDLE_INVALID;
  if (!_connection[idx].connected()) return BLE_CONN_HANDLE_INVALID;
  return _connection[idx].handle();
}

bool AdafruitBluefruit::connected(uint16_t conn_hdl) const
{
  int8_t slot = _slotOf(conn_hdl);
  return (slot >= 0) && _connection[slot].connected();
}

uint8_t AdafruitBluefruit::connected(void) const
{
  uint8_t n = 0;

  for (uint8_t i = 0; i < BLE_MAX_CONNECTION; i++) {
    if (_connection[i].connected()) n++;
  }
  return n;
}

uint16_t AdafruitBluefruit::attMtu(uint16_t conn_hdl) const
{
  int8_t slot = _slotOf(conn_hdl);
  return (slot < 0) ? BLE_GATT_ATT_MTU_DEFAULT : _connection[slot].getMtu();
}

bool BLEPeriph::connected(uint16_t conn_hdl)
{
  BLEConnection *conn = Bluefruit.Connection(conn_hdl);
  /* Connection() 은 끊긴 직후에도 객체를 주므로 connected() 를 따로 봐야 한다. */
  return (conn != NULL) && conn->connected() && (conn->getRole() == BLE_GAP_ROLE_PERIPH);
}

uint8_t BLEPeriph::connected(void)
{
  uint8_t n = 0;

  for (uint8_t i = 0; i < BLE_MAX_CONNECTION; i++) {
    uint16_t h = Bluefruit.connHandleAt(i);
    if (h != BLE_CONN_HANDLE_INVALID && this->connected(h)) n++;
  }
  return n;
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
  /*
   * gap_evt / gatts_evt / gattc_evt 는 모두 conn_handle 을 **첫 필드**로 두고,
   * common_evt 가 그 공용 앞자리다 (ble.h). 그래서 종류를 가리지 않고 여기서
   * 한 번에 꺼낼 수 있다. 연결과 무관한 이벤트면 BLE_CONN_HANDLE_INVALID 다.
   */
  const uint16_t conn_hdl = evt->evt.common_evt.conn_handle;

  switch (evt->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED: {
      /*
       * 빈 슬롯을 찾는다. **핸들 값과 슬롯 번호는 무관하다.**
       *
       * 슬롯이 없으면 연결을 끊는다. 그냥 두면 SoftDevice 는 링크를 유지하는데
       * 우리는 모르는 상태가 되고, 그 링크의 이벤트가 어디에도 안 닿는다.
       * 여기 걸린다는 건 SoftDevice 가 설정한 링크 수보다 많이 받았다는 뜻이라
       * SD_BLE_PERIPH_LINK_COUNT 와 begin() 인자를 봐야 한다.
       */
      int8_t slot = -1;
      for (uint8_t i = 0; i < BLE_MAX_CONNECTION; i++) {
        if (!_connection[i]._inUse()) { slot = (int8_t) i; break; }
      }
      if (slot < 0) {
        sd_ble_gap_disconnect(conn_hdl, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        break;
      }
      _conn_hdl = conn_hdl;
      _connection[slot]._begin(evt);

      /*
       * 연결이 맺어지면 SoftDevice 가 광고를 멈춘다. 자리가 남았어도
       * **여기서 자동으로 다시 켜지 않는다** — Adafruit 과 같은 동작이라
       * 스케치가 연결 콜백에서 Advertising.start(0) 을 부른다.
       */
      Advertising._setRunning(false);
#ifdef LED_CONN
      if (_auto_conn_led) ledOn(LED_CONN);
#endif
      _deferConnect(conn_hdl);
      break;
    }

    case BLE_GAP_EVT_DISCONNECTED: {
      /*
       * ⚠ 순서가 중요하다. **콜백 전에** 끊긴 것으로 표시해야 콜백 안의
       *   Bluefruit.connected() 가 이 링크를 빼고 센다. 스케치는 그 값으로
       *   광고를 다시 켤지 정한다 — 반대로 두면 항상 하나 많게 보인다.
       *   객체 자체는 콜백이 끝난 뒤에 반납하므로 콜백 안에서는 아직 읽힌다.
       */
      int8_t slot = _slotOf(conn_hdl);
      if (slot >= 0) _connection[slot]._disconnect();

      /* 콜백과 슬롯 반납은 콜백 태스크가 한다 (위 주석 참조). */
      _deferDisconnect(conn_hdl, evt->evt.gap_evt.params.disconnected.reason);

      /* 최근 연결이 끊겼으면 남아 있는 것 중 하나로 옮긴다. */
      if (_conn_hdl == conn_hdl) {
        _conn_hdl = BLE_CONN_HANDLE_INVALID;
        for (uint8_t i = 0; i < BLE_MAX_CONNECTION; i++) {
          if (_connection[i].connected()) { _conn_hdl = _connection[i].handle(); break; }
        }
      }
#ifdef LED_CONN
      /* 다중 연결에서는 **마지막 하나가 끊겨야** 끈다. */
      if (_auto_conn_led && connected() == 0) ledOff(LED_CONN);
#endif
      Advertising._restartIfNeeded();
      break;
    }

    /*
     * ⚠ 이 두 가지에 응답하지 않으면 연결이 조용히 끊긴다.
     *   SoftDevice 가 앱의 답을 기다리다 타임아웃을 낸다.
     */
    case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
      ble_gap_phys_t phys = { BLE_GAP_PHY_AUTO, BLE_GAP_PHY_AUTO };
      sd_ble_gap_phy_update(conn_hdl, &phys);
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
      sd_ble_gap_data_length_update(conn_hdl, NULL, NULL);
      break;

    /* 연결 파라미터 갱신 요청은 상대가 제안한 값을 그대로 받는다. */
    case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
      sd_ble_gap_conn_param_update(
          conn_hdl,
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

      sd_ble_gatts_exchange_mtu_reply(conn_hdl, ours);

      /* MTU 는 **연결마다 따로** 협상된다. 링크별로 보관해야 한다. */
      if (theirs < BLE_GATT_ATT_MTU_DEFAULT) theirs = BLE_GATT_ATT_MTU_DEFAULT;
      int8_t slot = _slotOf(conn_hdl);
      if (slot >= 0) {
        _connection[slot]._setMtu((theirs < ours) ? theirs : ours);
      }
      break;
    }

    /* 광고 duration 이 만료됐다 (연결로 끝난 경우는 CONNECTED 에서 처리된다). */
    case BLE_GAP_EVT_ADV_SET_TERMINATED:
      Advertising._onStopped();
      break;

    case BLE_GAP_EVT_RSSI_CHANGED: {
      int8_t slot = _slotOf(conn_hdl);
      if (slot >= 0) {
        _connection[slot]._setRssi(evt->evt.gap_evt.params.rssi_changed.rssi);
      }
      if (_rssi_cb) _rssi_cb(conn_hdl, evt->evt.gap_evt.params.rssi_changed.rssi);
      break;
    }

    /* notify 한 건이 무선으로 나갔다. **그 링크에서** 기다리던 쪽만 깨운다. */
    case BLE_GATTS_EVT_HVN_TX_COMPLETE: {
      int8_t slot = _slotOf(conn_hdl);
      if (slot >= 0 && _tx_sem[slot]) {
        xSemaphoreGive((SemaphoreHandle_t) _tx_sem[slot]);
      }
      break;
    }

    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
      /* 본딩을 안 하므로 시스템 속성이 없다. NULL 로 답한다. */
      sd_ble_gatts_sys_attr_set(conn_hdl, NULL, 0, 0);
      break;

    default:
      break;
  }

  Gatt._eventHandler(evt);
  Scanner._eventHandler(evt);

  /* characteristic 쓰기 이벤트 전달 */
  for (uint8_t i = 0; i < _char_count; i++) {
    if (_chars[i]) _chars[i]->_eventHandler(evt);
  }
}
