/*
 * BLECentral — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

BLECentral::BLECentral(void)
{
  memset(&_conn_param, 0, sizeof(_conn_param));
  _conn_param.min_conn_interval = 6;    /* 7.5 ms */
  _conn_param.max_conn_interval = 24;   /* 30 ms */
  _conn_param.slave_latency     = 0;
  _conn_param.conn_sup_timeout  = 400;  /* 4 초 */
}

void BLECentral::setConnInterval(uint16_t min, uint16_t max)
{
  _conn_param.min_conn_interval = min;
  _conn_param.max_conn_interval = max;
}

void BLECentral::setConnIntervalMS(uint16_t min_ms, uint16_t max_ms)
{
  /* 1.25 ms 단위. 내림하면 규격 최소치를 밑돌 수 있어 올림한다. */
  setConnInterval((uint16_t) ((min_ms * 4 + 4) / 5), (uint16_t) ((max_ms * 4 + 4) / 5));
}

bool BLECentral::connect(const ble_gap_evt_adv_report_t *report)
{
  if (report == NULL) return false;
  return connect(&report->peer_addr);
}

bool BLECentral::connect(const ble_gap_addr_t *peer_addr)
{
  if (peer_addr == NULL) return false;

  /*
   * 연결에 쓰는 스캔 파라미터. 스캐너의 것을 그대로 쓰되 active 와
   * filter_policy 는 SoftDevice 가 무시한다 (ble_gap.h 의 주석).
   */
  ble_gap_scan_params_t scan = *Bluefruit.Scanner.getParams();
  scan.timeout = 0;

  /*
   * ⚠ 우리가 설정한 연결 구성(MTU 등)을 쓰려면 **그 태그**로 연결해야 한다.
   *   기본 태그(0)로 연결하면 MTU 23 짜리 기본 구성이 쓰인다.
   */
  uint32_t err = sd_ble_gap_connect(peer_addr, &scan, &_conn_param, sdCentralConnCfgTag());

  /* 스캔은 SoftDevice 가 멈춘다. 라이브러리 상태도 맞춰 둔다. */
  if (err == NRF_SUCCESS) Bluefruit.Scanner._setStopped();

  return err == NRF_SUCCESS;
}

bool BLECentral::stopConnecting(void)
{
  return sd_ble_gap_connect_cancel() == NRF_SUCCESS;
}

bool BLECentral::connected(uint16_t conn_hdl)
{
  BLEConnection *conn = Bluefruit.Connection(conn_hdl);
  /* Connection() 은 끊긴 직후에도 객체를 주므로 connected() 를 따로 봐야 한다. */
  return (conn != NULL) && conn->connected() && (conn->getRole() == BLE_GAP_ROLE_CENTRAL);
}

uint8_t BLECentral::connected(void)
{
  uint8_t n = 0;

  for (uint8_t i = 0; i < BLE_MAX_CONNECTION; i++) {
    uint16_t h = Bluefruit.connHandleAt(i);
    if (h != BLE_CONN_HANDLE_INVALID && this->connected(h)) n++;
  }
  return n;
}
