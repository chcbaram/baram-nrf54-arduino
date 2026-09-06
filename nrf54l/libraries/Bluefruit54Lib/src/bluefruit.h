/*
 * bluefruit.h — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit Bluefruit52Lib 호환 진입점. 시그니처는 원본을 따르되
 * 구현은 S145 기준으로 새로 썼다 (CLAUDE.md R12 — 호환 우선).
 *
 * ⚠ 아직 전체가 아니다. 없는 것: 본딩/페어링(BLESecurity), central 역할,
 *   GATT 클라이언트(getPeerName 포함), HID, 실제 DFU.
 *   (docs/STATUS.md 의 B 단계 표 참조).
 */
#ifndef _BLUEFRUIT_H_
#define _BLUEFRUIT_H_

#include <Arduino.h>
#include <ble.h>
#include <ble_gap.h>

#include "sd_event_pump.h"

#include "BLEUuid.h"
#include "BLEService.h"
#include "BLECharacteristic.h"
#include "BLEConnection.h"

#define BLE_MAX_CHARS       32
#define BLE_ADV_BUF_MAX     BLE_GAP_ADV_SET_DATA_SIZE_MAX

/*
 * 동시 연결 수. 코어와 **같은 값이어야 한다** — SoftDevice 가 이보다 많은 링크를
 * 맺으면 라이브러리가 그 연결을 관리하지 못한다. 그래서 코어 헤더의 값을 그대로 쓴다.
 * 보드별 값은 boards.txt 의 build.extra_flags 에 있다.
 *
 * ⚠ 연결 핸들을 **배열 인덱스로 그대로 쓴다.** SoftDevice 가 핸들을
 *   [0, 링크수) 로 준다는 전제이며, Adafruit 도 같은 전제로 동작한다.
 *   그래도 범위 밖 핸들은 걸러낸다 — 틀렸을 때 조용히 남의 메모리를 밟지 않도록.
 */
#define BLE_MAX_CONNECTION  SD_BLE_PERIPH_LINK_COUNT

/*
 * 대역폭 프리셋. Adafruit 시그니처 호환을 위해 둔다.
 * 지금은 값을 받아 두기만 하고 실제 조정은 하지 않는다 — MTU 와 연결 이벤트
 * 길이는 sd_event_pump.c 의 컴파일 타임 설정으로 정해진다.
 */
typedef enum {
  BANDWIDTH_AUTO = 0,
  BANDWIDTH_LOW,
  BANDWIDTH_NORMAL,
  BANDWIDTH_HIGH,
  BANDWIDTH_MAX,
} ble_bandwidth_t;

typedef void (*ble_connect_callback_t)   (uint16_t conn_hdl);
typedef void (*ble_disconnect_callback_t)(uint16_t conn_hdl, uint8_t reason);
typedef void (*ble_adv_stop_callback_t)  (void);
typedef void (*ble_rssi_callback_t)      (uint16_t conn_hdl, int8_t rssi);

/* ── advertising 페이로드 빌더 ─────────────────────────────────────── */
class BLEAdvertisingData
{
  public:
    BLEAdvertisingData(void) : _count(0) { }

    bool addData(uint8_t type, const void *data, uint8_t len);
    bool addFlags(uint8_t flags);
    bool addTxPower(void);
    bool addName(void);
    bool addUuid(BLEUuid bleuuid);
    bool addService(BLEService &service);

    uint8_t        count(void) const { return _count; }
    uint8_t const *buffer(void) const { return _buf; }
    void           clearData(void) { _count = 0; }

  protected:
    uint8_t _buf[BLE_ADV_BUF_MAX];
    uint8_t _count;
};

class BLEAdvertising : public BLEAdvertisingData
{
  public:
    BLEAdvertising(void);

    void setInterval(uint16_t fast, uint16_t slow);   /* 0.625 ms 단위 */
    void setFastTimeout(uint16_t sec);
    void restartOnDisconnect(bool enable);

    /** advertising 종류. 기본은 연결 가능 + 스캔 응답. */
    void setType(uint8_t type) { _type = type; }

    /** advertising 이 멈추면 불린다 (duration 만료 등). */
    void setStopCallback(ble_adv_stop_callback_t fp) { _stop_cb = fp; }

    /* 코어 내부용 */
    void _onStopped(void) { _running = false; if (_stop_cb) _stop_cb(); }

    bool start(uint16_t timeout = 0);
    bool stop(void);
    bool isRunning(void) const { return _running; }

    /* 코어 내부용 */
    void _restartIfNeeded(void);
    void _setRunning(bool r) { _running = r; }

  protected:
    uint8_t  _handle;
    uint16_t _fast_interval;
    uint16_t _slow_interval;
    uint16_t _fast_timeout;
    bool     _restart;
    bool     _running;
    uint8_t  _type;
    ble_adv_stop_callback_t _stop_cb;
};

/* ── peripheral 역할 ───────────────────────────────────────────────── */
class BLEPeriph
{
  public:
    void setConnectCallback(ble_connect_callback_t fp)       { _connect_cb = fp; }
    void setDisconnectCallback(ble_disconnect_callback_t fp) { _disconnect_cb = fp; }

    /** peripheral 역할로 맺어진 연결 수. */
    uint8_t connected(void);
    bool    connected(uint16_t conn_hdl);

    /**
     * 선호 연결 간격. 단위는 1.25 ms 다 (BLE 규격).
     *
     * ⚠ 이건 **요청이지 명령이 아니다.** 실제 간격은 central 이 정한다.
     *   SoftDevice 의 PPCP 에 넣어 두면 상대가 참고한다.
     */
    void setConnInterval(uint16_t min, uint16_t max);
    void setConnIntervalMS(uint16_t min_ms, uint16_t max_ms);
    void setConnSupervisionTimeout(uint16_t timeout_10ms) { _sv_timeout = timeout_10ms; _applyPpcp(); }
    void setConnSlaveLatency(uint16_t latency)            { _latency = latency; _applyPpcp(); }

    void _applyPpcp(void);

    ble_connect_callback_t    _connect_cb    = NULL;
    ble_disconnect_callback_t _disconnect_cb = NULL;

    uint16_t _min_interval = 0;   /* 0 = 설정하지 않음 */
    uint16_t _max_interval = 0;
    uint16_t _latency      = 0;
    uint16_t _sv_timeout   = 400; /* 4 초 */
};

/* ── 싱글턴 ────────────────────────────────────────────────────────── */
class AdafruitBluefruit
{
  public:
    BLEAdvertising Advertising;
    BLEAdvertisingData ScanResponse;
    BLEPeriph      Periph;

    AdafruitBluefruit(void);

    /**
     * SoftDevice 와 BLE 스택을 켠다.
     *
     * @param prph_count    동시 peripheral 연결 수. 1 ~ BLE_MAX_CONNECTION.
     * @param central_count central 역할 연결 수. **0 만 지원한다.**
     *
     * ⚠ 지원하지 않는 값을 주면 **false 를 돌려준다** — 조용히 깎지 않는다.
     *   그래야 스케치가 왜 두 번째 연결이 안 되는지 헤매지 않는다.
     *   BLE_MAX_CONNECTION 은 보드가 정한다 (boards.txt).
     */
    bool begin(uint8_t prph_count = 1, uint8_t central_count = 0);

    /** 맺어진 연결 수. `if (Bluefruit.connected())` 로도 그대로 쓴다. */
    uint8_t connected(void) const;
    bool    connected(uint16_t conn_hdl) const;

    /**
     * 가장 최근에 맺어진 연결의 핸들. 하나도 없으면 BLE_CONN_HANDLE_INVALID.
     *
     * ⚠ 핸들을 받지 않는 write()/notify() 는 **전체가 아니라 여기로만** 간다.
     *   모든 연결에 보내려면 스케치가 핸들을 돌며 불러야 한다
     *   (examples/Peripheral/bleuart_multi 참고). Adafruit 과 같은 동작이다.
     */
    uint16_t connHandle(void) const { return _conn_hdl; }

    /** 그 연결에서 협상된 ATT MTU. 연결 전이거나 없는 핸들이면 기본값(23)이다. */
    uint16_t attMtu(uint16_t conn_hdl) const;
    uint16_t attMtu(void) const { return attMtu(_conn_hdl); }

    /** 한 번에 보낼 수 있는 최대 페이로드 (MTU − ATT 헤더 3). */
    uint16_t maxPayload(uint16_t conn_hdl) const { return (uint16_t)(attMtu(conn_hdl) - 3); }
    uint16_t maxPayload(void) const { return maxPayload(_conn_hdl); }

    /** 연결 핸들로 BLEConnection 을 얻는다. 연결돼 있지 않으면 NULL. */
    BLEConnection *Connection(uint16_t conn_hdl);

    /**
     * 연결되면 LED_CONN 을 켠다.
     * ⚠ variant 에 LED 가 하나뿐이면 LED_BUILTIN 과 같은 핀이다
     *   (XIAO 가 그렇다). 스케치의 blink 와 겹쳐 보일 수 있다.
     */
    void autoConnLed(bool enable);

    /**
     * 연결 LED 깜빡임 주기 (ms).
     *
     * ⚠ Adafruit 은 advertising 중에 LED 를 이 주기로 깜빡인다. 우리는 아직
     *   깜빡이지 않고 연결 상태만 켜고 끈다 — 값은 받아 두고 무시한다.
     *   깜빡임을 넣으려면 타이머 태스크가 필요하다.
     */
    void setConnLedInterval(uint32_t ms) { _conn_led_interval = ms; }

    /** RSSI 가 바뀌면 불린다. BLEConnection::monitorRssi() 로 시작해야 온다. */
    void setRssiCallback(ble_rssi_callback_t fp) { _rssi_cb = fp; }

    /** Adafruit 시그니처 호환. 값만 받아 둔다 (위 ble_bandwidth_t 주석 참조). */
    void configPrphBandwidth(ble_bandwidth_t bw) { _bandwidth = bw; }
    void configPrphConn(uint16_t mtu, uint8_t event_len, uint8_t hvn_qsize, uint8_t wrcmd_qsize);

    void setName(const char *name);
    const char *getName(void) const { return _name; }
    bool setTxPower(int8_t power);

    bool disconnect(uint16_t conn_hdl);

    /* 서비스/characteristic 등록 — 코어 내부용 */
    void     _setCurrentService(BLEService *svc) { _cur_service = svc; }
    uint16_t _currentServiceHandle(void) const
    {
      return _cur_service ? _cur_service->handle() : BLE_GATT_HANDLE_INVALID;
    }
    bool _registerChar(BLECharacteristic *chr);

    /* 이벤트 진입점 (sd_event_pump 가 부른다) */
    void _eventHandler(const ble_evt_t *evt);

    /**
     * notify 송신 버퍼가 빌 때까지 기다린다.
     *
     * SoftDevice 의 notify 큐는 얕다 (기본 1). 연속으로 보내면 금방
     * NRF_ERROR_RESOURCES 가 나고, 그때 그냥 포기하면 **데이터가 조용히
     * 사라진다.** BLE_GATTS_EVT_HVN_TX_COMPLETE 를 기다렸다 재시도해야 한다.
     *
     * ⚠ 세마포어는 **연결마다 따로**다. 하나로 두면 A 링크의 완료가 B 링크
     *   대기자를 깨워, 자리가 안 생겼는데 재시도하고 헛되이 횟수만 쓴다.
     *
     * @return 자리가 생겼으면 true, 시간이 다 됐으면 false.
     */
    bool _waitTxComplete(uint16_t conn_hdl, uint32_t ms);

  protected:
    char        _name[32];
    uint16_t    _conn_hdl;    /* 가장 최근 연결 */
    uint8_t     _prph_count;
    BLEService *_cur_service;
    BLECharacteristic *_chars[BLE_MAX_CHARS];
    uint8_t     _char_count;
    bool        _begun;
    /* SemaphoreHandle_t. 헤더에 FreeRTOS 를 끌어들이지 않으려고 void* 로 둔다. */
    void       *_tx_sem[BLE_MAX_CONNECTION];
    BLEConnection _connection[BLE_MAX_CONNECTION];
    bool        _auto_conn_led;
    uint32_t    _conn_led_interval;
    ble_bandwidth_t _bandwidth;
    ble_rssi_callback_t _rssi_cb;
};

extern AdafruitBluefruit Bluefruit;

/*
 * 서비스들을 여기서 함께 끌어온다.
 *
 * Adafruit 예제는 `#include <bluefruit.h>` **한 줄만** 쓰고 BLEUart / BLEDis /
 * BLEBas / BLEDfu 를 바로 선언한다. 원본 bluefruit.h 가 그렇게 하기 때문이다.
 * 따로 include 하게 만들면 예제를 고쳐야 하므로 (R12 — 호환 우선) 같은 구조로 둔다.
 *
 * 순서 주의: 이 헤더들은 위의 클래스 선언에 의존하므로 파일 끝에 와야 한다.
 */
#include "BLEUart.h"
#include "BLEDis.h"
#include "BLEBas.h"
#include "BLEDfu.h"

#endif
