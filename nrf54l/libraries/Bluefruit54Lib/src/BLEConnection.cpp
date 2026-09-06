/*
 * BLEConnection — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "BLEConnection.h"
#include <string.h>

BLEConnection::BLEConnection(void)
{
  _conn_hdl  = BLE_CONN_HANDLE_INVALID;
  _connected = false;
  _role      = BLE_GAP_ROLE_INVALID;
  _att_mtu   = BLE_GATT_ATT_MTU_DEFAULT;
  _rssi      = 0;
  memset(&_peer_addr, 0, sizeof(_peer_addr));
}

bool BLEConnection::monitorRssi(uint8_t threshold_dbm)
{
  if (!_connected) return false;
  /* skip_count = 0 : 걸러내지 않는다. */
  return sd_ble_gap_rssi_start(_conn_hdl, threshold_dbm, 0) == NRF_SUCCESS;
}

bool BLEConnection::stopRssi(void)
{
  if (!_connected) return false;
  return sd_ble_gap_rssi_stop(_conn_hdl) == NRF_SUCCESS;
}

void BLEConnection::_begin(const ble_evt_t *evt)
{
  _conn_hdl  = evt->evt.gap_evt.conn_handle;
  _connected = true;
  _role      = evt->evt.gap_evt.params.connected.role;
  _att_mtu   = BLE_GATT_ATT_MTU_DEFAULT;   /* 협상 전 */
  _peer_addr = evt->evt.gap_evt.params.connected.peer_addr;
}

void BLEConnection::_end(void)
{
  _conn_hdl  = BLE_CONN_HANDLE_INVALID;
  _connected = false;
  _att_mtu   = BLE_GATT_ATT_MTU_DEFAULT;
}

bool BLEConnection::getPeerName(char *buf, uint16_t bufsize)
{
  if (buf && bufsize) buf[0] = 0;
  return false;      /* 미구현 — 헤더 주석 참조 */
}

bool BLEConnection::disconnect(void)
{
  if (!_connected) return false;
  return sd_ble_gap_disconnect(_conn_hdl,
                               BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) == NRF_SUCCESS;
}
