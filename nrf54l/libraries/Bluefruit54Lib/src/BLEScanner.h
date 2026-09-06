/*
 * BLEScanner — 주변 광고를 훑는다 (observer / central 준비 단계)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Adafruit Bluefruit52Lib 을 따른다 (CLAUDE.md R12 — 호환 우선).
 */
#ifndef _BLE_SCANNER_H_
#define _BLE_SCANNER_H_

#include <Arduino.h>
#include <ble.h>
#include <ble_gap.h>
#include "BLEUuid.h"

#define BLE_SCAN_INTERVAL_DFLT    160   /* 100 ms (0.625 ms 단위) */
#define BLE_SCAN_WINDOW_DFLT      80    /*  50 ms */

/* UUID 필터 최대 개수. 고정 배열이다 — 런타임 할당을 쓰지 않는다. */
#ifndef BLE_SCAN_FILTER_UUID_MAX
#define BLE_SCAN_FILTER_UUID_MAX  (4)
#endif

class BLEService;
class BLEClientService;

class BLEScanner
{
  public:
    typedef void (*rx_callback_t)  (ble_gap_evt_adv_report_t *report);
    typedef void (*stop_callback_t)(void);

    BLEScanner(void);

    ble_gap_scan_params_t *getParams(void) { return &_param; }
    bool isRunning(void) const { return _running; }

    /** 스캔 요청을 보내 스캔 응답까지 받는다. 전력을 더 쓴다. */
    void useActiveScan(bool enable) { _param.active = enable ? 1 : 0; }

    void setInterval(uint16_t interval, uint16_t window);   /* 0.625 ms 단위 */
    void setIntervalMS(uint16_t interval_ms, uint16_t window_ms);

    /** 연결이 끊기면 스캔을 다시 시작한다. */
    void restartOnDisconnect(bool enable) { _restart = enable; }

    /* ── 필터 ────────────────────────────────────────────────────────
     * 필터는 **콜백을 부르기 전에** 우리가 거른다. SoftDevice 가 아니라
     * 여기서 거르므로 전파는 그대로 받는다 — 전력이 줄지는 않는다.
     */
    void filterRssi(int8_t min_rssi) { _filter_rssi = min_rssi; }
    void filterMSD(uint16_t manuf_id);
    void filterUuid(BLEUuid uuid);
    void filterUuid(BLEUuid uuid1, BLEUuid uuid2);
    void filterUuid(BLEUuid uuid[], uint8_t count);
    void filterService(BLEService &svc);
    void filterService(BLEClientService &svc);
    void clearFilters(void);

    bool start(uint16_t timeout = 0);    /* timeout 은 10 ms 단위, 0 = 무한 */

    /**
     * 보고를 하나 받은 뒤 스캔을 이어 간다.
     *
     * ⚠ **SoftDevice v6 이후로는 보고 하나마다 스캔이 멈춘다.** 콜백 안에서
     *   resume() 을 부르지 않으면 그 뒤로 아무 보고도 안 온다.
     *   증상이 "처음 하나만 잡히고 조용하다" 로 나타난다.
     */
    bool resume(void);
    bool stop(void);

    void setRxCallback(rx_callback_t fp)     { _rx_cb = fp; }
    void setStopCallback(stop_callback_t fp) { _stop_cb = fp; }

    /* ── 광고 데이터 파서 ───────────────────────────────────────────── */

    /**
     * 광고 데이터에서 AD 타입 하나를 뽑는다.
     * @return 찾은 값의 길이. 없으면 0.
     */
    uint8_t parseReportByType(const uint8_t *data, uint8_t len, uint8_t type,
                              uint8_t *buf, uint8_t bufsize = 0);
    uint8_t parseReportByType(const ble_gap_evt_adv_report_t *report, uint8_t type,
                              uint8_t *buf, uint8_t bufsize = 0);

    bool checkReportForUuid(const ble_gap_evt_adv_report_t *report, BLEUuid uuid);
    bool checkReportForService(const ble_gap_evt_adv_report_t *report, BLEService &svc);
    bool checkReportForService(const ble_gap_evt_adv_report_t *report, BLEClientService &svc);

    /* 코어 내부용 */
    void _eventHandler(const ble_evt_t *evt);
    /** 연결을 시작하면 SoftDevice 가 스캔을 멈춘다 — 그 사실을 반영한다. */
    void _setStopped(void) { _running = false; }

  protected:
    /* SoftDevice 가 보고를 써 넣는 버퍼. 스캔이 도는 동안 살아 있어야 한다. */
    uint8_t    _scan_data[BLE_GAP_SCAN_BUFFER_MAX];
    ble_data_t _report_data;
    ble_gap_scan_params_t _param;

    bool     _running;
    bool     _restart;

    int8_t   _filter_rssi;
    bool     _filter_msd_en;
    uint16_t _filter_msd_id;
    BLEUuid  _filter_uuid[BLE_SCAN_FILTER_UUID_MAX];
    uint8_t  _filter_uuid_count;

    rx_callback_t   _rx_cb;
    stop_callback_t _stop_cb;

    bool passesFilters(const ble_gap_evt_adv_report_t *report);
};

#endif
