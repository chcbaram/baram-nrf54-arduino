/*
 * BLECentral — 우리가 먼저 연결을 건다
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Adafruit Bluefruit52Lib 을 따른다 (CLAUDE.md R12 — 호환 우선).
 */
#ifndef _BLE_CENTRAL_H_
#define _BLE_CENTRAL_H_

#include <Arduino.h>
#include <ble.h>
#include <ble_gap.h>

class BLECentral
{
  public:
    BLECentral(void);

    /**
     * 스캔 보고에 실린 주소로 연결한다.
     *
     * ⚠ 연결을 시작하면 **SoftDevice 가 스캔을 멈춘다.** 스캔을 계속하려면
     *   연결 콜백에서 `Bluefruit.Scanner.start(0)` 을 다시 불러야 한다.
     * ⚠ 이 함수는 요청만 넣고 바로 돌아온다. 실제 연결은 연결 콜백으로 온다.
     */
    bool connect(const ble_gap_evt_adv_report_t *report);
    bool connect(const ble_gap_addr_t *peer_addr);

    /** 진행 중인 연결 시도를 취소한다. */
    bool stopConnecting(void);

    bool    connected(uint16_t conn_hdl);
    uint8_t connected(void);

    void setConnectCallback(ble_connect_callback_t fp)       { _connect_cb = fp; }
    void setDisconnectCallback(ble_disconnect_callback_t fp) { _disconnect_cb = fp; }

    /** 선호 연결 간격 (1.25 ms 단위). central 은 이 값이 **실제로 쓰인다.** */
    void setConnInterval(uint16_t min, uint16_t max);
    void setConnIntervalMS(uint16_t min_ms, uint16_t max_ms);
    void setConnSupervisionTimeout(uint16_t timeout_10ms) { _conn_param.conn_sup_timeout = timeout_10ms; }
    void setConnSlaveLatency(uint16_t latency)            { _conn_param.slave_latency = latency; }

    ble_connect_callback_t    _connect_cb    = NULL;
    ble_disconnect_callback_t _disconnect_cb = NULL;

  protected:
    ble_gap_conn_params_t _conn_param;
};

#endif
