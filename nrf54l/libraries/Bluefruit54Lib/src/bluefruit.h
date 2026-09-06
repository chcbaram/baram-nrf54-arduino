/*
 * bluefruit.h — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit Bluefruit52Lib 호환 진입점. 시그니처는 원본을 따르되
 * 구현은 S145 기준으로 새로 썼다 (CLAUDE.md R12 — 호환 우선).
 *
 * ⚠ 아직 전체가 아니다. 지금 있는 것: Bluefruit.begin(), 이름/TX 파워,
 *   GATT 서비스·characteristic, advertising, 연결/해제 콜백.
 *   BLEUart / BLEDis / BLEBas / 본딩 / BLEDfu 는 아직 없다
 *   (docs/STATUS.md 의 B 단계 표 참조).
 */
#ifndef _BLUEFRUIT_H_
#define _BLUEFRUIT_H_

#include <Arduino.h>
#include <ble.h>
#include <ble_gap.h>

#include "BLEUuid.h"
#include "BLEService.h"
#include "BLECharacteristic.h"

#define BLE_MAX_CHARS       32
#define BLE_ADV_BUF_MAX     BLE_GAP_ADV_SET_DATA_SIZE_MAX

typedef void (*ble_connect_callback_t)   (uint16_t conn_hdl);
typedef void (*ble_disconnect_callback_t)(uint16_t conn_hdl, uint8_t reason);

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
};

/* ── peripheral 역할 ───────────────────────────────────────────────── */
class BLEPeriph
{
  public:
    void setConnectCallback(ble_connect_callback_t fp)       { _connect_cb = fp; }
    void setDisconnectCallback(ble_disconnect_callback_t fp) { _disconnect_cb = fp; }

    ble_connect_callback_t    _connect_cb    = NULL;
    ble_disconnect_callback_t _disconnect_cb = NULL;
};

/* ── 싱글턴 ────────────────────────────────────────────────────────── */
class AdafruitBluefruit
{
  public:
    BLEAdvertising Advertising;
    BLEAdvertisingData ScanResponse;
    BLEPeriph      Periph;

    AdafruitBluefruit(void);

    bool begin(uint8_t prph_count = 1, uint8_t central_count = 0);
    bool connected(void) const { return _conn_hdl != BLE_CONN_HANDLE_INVALID; }
    uint16_t connHandle(void) const { return _conn_hdl; }

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
     * @return 자리가 생겼으면 true, 시간이 다 됐으면 false.
     */
    bool _waitTxComplete(uint32_t ms);

  protected:
    char        _name[32];
    uint16_t    _conn_hdl;
    BLEService *_cur_service;
    BLECharacteristic *_chars[BLE_MAX_CHARS];
    uint8_t     _char_count;
    bool        _begun;
    void       *_tx_sem;      /* SemaphoreHandle_t. 헤더에 FreeRTOS 를 끌어들이지 않는다 */
};

extern AdafruitBluefruit Bluefruit;

#endif
