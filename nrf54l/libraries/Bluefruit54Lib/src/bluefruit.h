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
#include "BLEScanner.h"

#define BLE_MAX_CHARS       32

/** 클라이언트로 등록할 수 있는 상대 서비스 수. 고정 배열이다. */
#ifndef BLE_MAX_CLIENT_SERVICE
#define BLE_MAX_CLIENT_SERVICE  (8)
#endif
#define BLE_ADV_BUF_MAX     BLE_GAP_ADV_SET_DATA_SIZE_MAX

/*
 * 동시 연결 수 = 연결 슬롯 수. 코어와 **같은 값이어야 한다** — SoftDevice 가
 * 이보다 많은 링크를 맺으면 라이브러리가 그 연결을 관리하지 못한다.
 * 보드별 값은 boards.txt 의 build.extra_flags 에 있다.
 *
 * ⚠ **연결 핸들은 배열 인덱스가 아니다.** 링크 수보다 큰 핸들이 온다.
 *   실기에서 확인했다: 링크 수를 4 로 두고도 SoftDevice 가 핸들 4 를 줬다.
 *   핸들은 슬롯 번호가 아니라 연결마다 새로 매기는 번호에 가깝다.
 *
 *   Adafruit 이 이 문제를 안 겪는 건 BLE_MAX_CONNECTION 을 설정된 링크 수와
 *   무관하게 **20**(SoftDevice 최대)으로 잡아 배열을 넉넉히 두기 때문이다.
 *   우리는 그 대신 핸들 -> 슬롯 매핑을 쓴다. 슬롯이 4개면 4개만 있으면 된다.
 *   공개 API 는 그대로 핸들을 받으므로 스케치 쪽은 차이가 없다.
 *
 * ⚠ 그래서 `for (h = 0; h < BLE_MAX_CONNECTION; h++)` 로 연결을 훑으면 안 된다.
 *   connHandleAt() 을 쓴다.
 *
 * ⚠ **역할 배분은 컴파일 타임이 아니라 `begin(prph, central)` 이 정한다.**
 *   RAM 경계만 링커가 고정하고, 그 안에서 어떻게 나눌지는 스케치의 몫이다
 *   (Adafruit 과 같다). 그래서 같은 보드가 `begin(4,0)` 도 `begin(0,4)` 도 된다 —
 *   둘 다 링크 4개분이라 같은 RAM 을 쓴다. 안 들어가면 begin() 이 실패한다.
 *
 *   boards.txt 의 `-DSD_BLE_*_LINK_COUNT` 는 **인자 없는 `begin()` 의 기본값**일
 *   뿐이고 상한이 아니다.
 *
 * BLE_MAX_CONNECTION 은 연결 **슬롯 배열의 크기**다 (Adafruit 도 이 상수를
 * 그렇게 쓴다 — 그쪽은 20 으로 고정). 실제 링크 수보다 크면 남는 슬롯은
 * 쓰이지 않는다. 슬롯 하나가 앱 RAM 에서 수십 바이트라 넉넉히 잡아도 싸다.
 */
#ifndef BLE_MAX_CONNECTION
#define BLE_MAX_CONNECTION          (8)
#endif

/** 인자 없는 begin() 이 쓰는 기본값. 보드가 boards.txt 에서 정한다. */
#define BLE_DEFAULT_PERIPH_COUNT    SD_BLE_PERIPH_LINK_COUNT
#define BLE_DEFAULT_CENTRAL_COUNT   SD_BLE_CENTRAL_LINK_COUNT

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

#include "BLECentral.h"

class BLEClientService;   /* 아래 include 에서 정의된다 (순환 의존) */

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

class BLEBeacon;
class EddyStoneUrl;

class BLEAdvertising : public BLEAdvertisingData
{
  public:
    BLEAdvertising(void);

    /**
     * 광고 페이로드를 비콘으로 채운다.
     *
     * ⚠ 기존 페이로드를 **지우고** 다시 쓴다. 비콘 페이로드가 25바이트라
     *   31바이트 광고 패킷에 다른 것을 같이 넣을 자리가 사실상 없다.
     *   이름은 스캔 응답(ScanResponse)에 넣는다.
     */
    bool setBeacon(BLEBeacon &beacon);
    bool setBeacon(EddyStoneUrl &eddy_url);

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

/* ── GATT 클라이언트 ───────────────────────────────────────────────── */

/** GATT 절차 기본 대기 한도 (ms). 상대가 느려도 이 정도면 온다. */
#define BLE_GATT_TIMEOUT_MS   (3000)

/**
 * 상대(peer)의 GATT 서버를 읽는다.
 *
 * peripheral 링크에서도 쓸 수 있다 — GATT 의 클라이언트/서버는 연결의
 * central/peripheral 역할과 **별개**다. 우리가 peripheral 이어도 상대의
 * 서비스를 읽을 수 있고, getPeerName() 이 그 경우다.
 *
 * ⚠ 여기 함수들은 **블로킹**이다. 응답 이벤트를 기다린다.
 *   그래서 BLE 이벤트 태스크에서 부르면 안 된다 — 기다리는 응답을 처리할
 *   주체가 자기 자신이라 영영 안 온다. 연결/해제 콜백은 별도 태스크에서
 *   돌리므로(AdafruitBluefruit 의 콜백 태스크) 콜백 안에서는 안전하다.
 */
class BLEGatt
{
  public:
    BLEGatt(void);

    /**
     * UUID 로 characteristic 값을 읽는다. 핸들을 몰라도 된다 —
     * SoftDevice 가 탐색과 읽기를 한 번에 한다.
     *
     * @return 읽은 바이트 수. 실패하면 0.
     */
    uint16_t readCharByUuid(uint16_t conn_hdl, BLEUuid bleuuid,
                            void *buffer, uint16_t bufsize,
                            uint16_t start_hdl = 1, uint16_t end_hdl = 0xFFFF);

    /**
     * 상대의 primary 서비스를 UUID 로 찾는다.
     * @param[out] start_hdl,end_hdl 그 서비스의 핸들 범위
     * @return 찾으면 true.
     */
    bool discoverService(uint16_t conn_hdl, BLEUuid bleuuid,
                         uint16_t *start_hdl, uint16_t *end_hdl);

    /**
     * 핸들 범위 안의 characteristic 을 훑는다.
     *
     * ⚠ 한 번의 응답에 다 안 올 수 있다. 마지막 핸들 다음부터 다시 물어
     *   **범위가 끝날 때까지 반복해야 한다.** 한 번만 부르고 끝내면
     *   뒤쪽 characteristic 이 조용히 빠진다.
     *
     * @return 채운 개수.
     */
    uint8_t discoverChars(uint16_t conn_hdl, uint16_t start_hdl, uint16_t end_hdl,
                          ble_gattc_char_t *out, uint8_t max_count);

    /** value 핸들 뒤에서 CCCD(0x2902) 를 찾는다. 없으면 0. */
    uint16_t discoverCccd(uint16_t conn_hdl, uint16_t value_hdl, uint16_t end_hdl);

    /**
     * 핸들을 알고 있는 상대 characteristic 을 읽는다.
     * @return 읽은 바이트 수. 실패하면 0.
     */
    uint16_t readChar(uint16_t conn_hdl, uint16_t value_hdl, void *buffer, uint16_t bufsize);

    /** 상대 characteristic 에 쓴다. resp=true 면 응답을 기다린다. */
    bool writeChar(uint16_t conn_hdl, uint16_t value_hdl,
                   const void *data, uint16_t len, bool resp);

    /**
     * ATT MTU 교환을 **우리가 먼저** 건다.
     *
     * ⚠ central 은 이걸 안 하면 MTU 가 23 에 머문다. peripheral 일 때는 보통
     *   상대가 걸어 주지만, central 일 때는 걸어 주는 쪽이 없다.
     *   실제로 이 호출을 넣기 전에는 central 연결이 MTU 23 이었다.
     *
     * @return 협상된 MTU. 실패하면 0.
     */
    uint16_t exchangeMtu(uint16_t conn_hdl);

    /* 코어 내부용 */
    bool _begin(void);
    void _eventHandler(const ble_evt_t *evt);

  protected:
    /* 진행 중인 절차. GATTC 는 링크당 한 번에 하나라 한 건만 추적하면 된다. */
    enum {
      PROC_NONE = 0,
      PROC_READ_UUID,
      PROC_SRVC,
      PROC_CHAR,
      PROC_DESC,
      PROC_WRITE,
      PROC_MTU,
      PROC_READ,
    };

    void    *_sem;          /* SemaphoreHandle_t */
    void    *_mutex;        /* 절차 하나씩 직렬화 */
    uint8_t  _proc;
    uint16_t _conn_hdl;

    /* 읽기 결과 */
    uint8_t *_buf;
    uint16_t _bufsize;
    uint16_t _len;

    /* 탐색 결과 */
    uint16_t _srvc_start;
    uint16_t _srvc_end;
    ble_gattc_char_t *_chars;
    uint8_t  _char_max;
    uint8_t  _char_count;
    uint16_t _cccd_hdl;
    uint16_t _mtu;
    bool     _ok;

    bool beginProc(uint8_t proc, uint16_t conn_hdl);
    bool waitProc(void);
    void endProc(void);
};

/* ── 싱글턴 ────────────────────────────────────────────────────────── */
class AdafruitBluefruit
{
  public:
    BLEAdvertising Advertising;
    BLEAdvertisingData ScanResponse;
    BLEPeriph      Periph;
    BLECentral     Central;
    BLEGatt        Gatt;
    BLEScanner     Scanner;

    AdafruitBluefruit(void);

    /**
     * SoftDevice 와 BLE 스택을 켠다.
     *
     * @param prph_count    동시 peripheral 연결 수.
     * @param central_count 동시 central 연결 수.
     *
     * ⚠ 상한은 **RAM 이 정한다.** 링커가 떼어 준 SoftDevice 영역에 안 들어가면
     *   **false 를 돌려준다** — 조용히 깎지 않는다. 그래야 스케치가 왜 N번째
     *   연결이 안 되는지 헤매지 않는다. 얼마가 필요했는지는 `sdCfgResults()`
     *   의 ram_required 로 볼 수 있다.
     *
     * ⚠ config*() 계열은 **이 함수보다 먼저** 불러야 한다. 여기서 값이 확정된다.
     */
    bool begin(uint8_t prph_count = BLE_DEFAULT_PERIPH_COUNT,
               uint8_t central_count = BLE_DEFAULT_CENTRAL_COUNT);

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

    /**
     * idx 번째 **슬롯**의 연결 핸들. 비어 있으면 BLE_CONN_HANDLE_INVALID.
     *
     * 붙어 있는 연결을 전부 훑을 때 쓴다. 핸들이 0..N-1 이 아니므로 핸들 값으로
     * 반복하면 놓친다 (위 BLE_MAX_CONNECTION 주석 참조).
     *
     *   for (uint8_t i = 0; i < BLE_MAX_CONNECTION; i++) {
     *     uint16_t h = Bluefruit.connHandleAt(i);
     *     if (h == BLE_CONN_HANDLE_INVALID) continue;
     *     ...
     *   }
     */
    uint16_t connHandleAt(uint8_t idx) const;

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

    /**
     * 대역폭 프리셋. **begin() 보다 먼저** 불러야 한다.
     * 값은 Adafruit 과 같다 (구현 주석 참조).
     */
    void configPrphBandwidth(ble_bandwidth_t bw);
    void configCentralBandwidth(ble_bandwidth_t bw) { configPrphBandwidth(bw); }

    /**
     * 연결 구성을 직접 준다. **begin() 보다 먼저** 불러야 한다.
     *
     * ⚠ S145 는 연결 구성을 하나만 허용하므로 peripheral 과 central 이
     *   **같은 값을 공유한다.** Adafruit 처럼 역할별로 나눌 수 없다.
     * ⚠ wrcmd_qsize 는 아직 반영하지 않는다 (GATTC write command 큐).
     */
    void configPrphConn(uint16_t mtu, uint16_t event_len, uint8_t hvn_qsize, uint8_t wrcmd_qsize);
    void configCentralConn(uint16_t mtu, uint16_t event_len, uint8_t hvn_qsize, uint8_t wrcmd_qsize)
    {
      configPrphConn(mtu, event_len, hvn_qsize, wrcmd_qsize);
    }

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

    /** 핸들이 들어 있는 슬롯. 없으면 -1. */
    int8_t _slotOf(uint16_t conn_hdl) const;

    /* 이벤트 진입점 (sd_event_pump 가 부른다) */
    void _eventHandler(const ble_evt_t *evt);

    /** 클라이언트 서비스가 GATTC 이벤트를 받으려면 등록해야 한다. */
    bool _registerClientService(BLEClientService *svc);

    /**
     * 스케치 콜백을 실행할 태스크에 넘긴다.
     *
     * ⚠ 콜백을 BLE 이벤트 태스크에서 **직접 부르면 안 된다.** 상류 예제는
     *   연결 콜백 안에서 getPeerName() 같은 블로킹 GATT 호출을 한다. 그걸
     *   이벤트 태스크에서 하면 응답 이벤트를 처리할 주체가 자기 자신이라
     *   영영 안 온다. Adafruit 이 ada_callback() 으로 미루는 이유가 그것이다.
     */
    void _deferConnect(uint16_t conn_hdl, uint8_t role);
    void _deferDisconnect(uint16_t conn_hdl, uint8_t reason, uint8_t role);
    void _callbackTask(void);      /* 위 태스크의 본체 */

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
    uint8_t     _central_count;
    sd_ble_conf_t _conf;       /* begin() 에 넘길 구성 */
    BLEService *_cur_service;
    BLECharacteristic *_chars[BLE_MAX_CHARS];
    uint8_t     _char_count;
    BLEClientService *_client_svcs[BLE_MAX_CLIENT_SERVICE];
    uint8_t     _client_svc_count;
    bool        _begun;
    /* SemaphoreHandle_t. 헤더에 FreeRTOS 를 끌어들이지 않으려고 void* 로 둔다. */
    void       *_tx_sem[BLE_MAX_CONNECTION];
    BLEConnection _connection[BLE_MAX_CONNECTION];
    void       *_cb_queue;    /* QueueHandle_t. 콜백 지연 실행 */
    void       *_cb_task;     /* TaskHandle_t */
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
#include "bonding.h"
#include "BLEClientService.h"
#include "BLEClientCharacteristic.h"
#include "BLEClientUart.h"
#include "BLEClientBas.h"
#include "BLEClientDis.h"
#include "BLEBeacon.h"
#include "BLEDis.h"
#include "BLEBas.h"
#include "BLEDfu.h"

#endif
