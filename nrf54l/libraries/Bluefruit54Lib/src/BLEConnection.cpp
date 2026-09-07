/*
 * BLEConnection — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "BLEConnection.h"
#include "bluefruit.h"
#include "sd_event_pump.h"
#include <string.h>

BLEConnection::BLEConnection(void)
{
  _conn_hdl  = BLE_CONN_HANDLE_INVALID;
  _in_use    = false;
  _connected = false;
  _secured   = false;
  _role      = BLE_GAP_ROLE_INVALID;
  _att_mtu   = BLE_GATT_ATT_MTU_DEFAULT;
  _rssi      = 0;
  _phy       = BLE_GAP_PHY_1MBPS;
  _conn_interval = 0;
  _slave_latency = 0;
  _sup_timeout   = 0;
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
  _in_use    = true;
  _connected = true;
  _secured   = false;
  _role      = evt->evt.gap_evt.params.connected.role;
  _att_mtu   = BLE_GATT_ATT_MTU_DEFAULT;   /* 협상 전 */
  _rssi      = 0;
  _phy       = BLE_GAP_PHY_1MBPS;          /* 협상 전 */
  _peer_addr = evt->evt.gap_evt.params.connected.peer_addr;
  _setConnParams(&evt->evt.gap_evt.params.connected.conn_params);
}

void BLEConnection::_setConnParams(const ble_gap_conn_params_t *p)
{
  _conn_interval = p->max_conn_interval;
  _slave_latency = p->slave_latency;
  _sup_timeout   = p->conn_sup_timeout;
}

void BLEConnection::_disconnect(void)
{
  _connected = false;
}

void BLEConnection::_end(void)
{
  _conn_hdl  = BLE_CONN_HANDLE_INVALID;
  _in_use    = false;
  _connected = false;
  _secured   = false;
  _role      = BLE_GAP_ROLE_INVALID;
  _att_mtu   = BLE_GATT_ATT_MTU_DEFAULT;
  _phy       = BLE_GAP_PHY_1MBPS;
}

uint16_t BLEConnection::getPeerName(char *buf, uint16_t bufsize)
{
  if (buf == NULL || bufsize == 0) return 0;
  buf[0] = 0;
  if (!_connected) return 0;

  /* 널 자리를 남겨 둔다 — 스케치가 그대로 print 한다. */
  uint16_t len = Bluefruit.Gatt.readCharByUuid(_conn_hdl,
                                               BLEUuid(UUID16_CHR_DEVICE_NAME),
                                               buf, (uint16_t) (bufsize - 1));
  buf[len] = 0;
  return len;
}

bool BLEConnection::bonded(void)
{
  if (!_connected) return false;

  bond_keys_t keys;
  return bondLoadKeys(_role, &_peer_addr, &keys);
}

bool BLEConnection::requestPairing(void)
{
  if (!_connected) return false;
  return Bluefruit.Security.authenticate(_conn_hdl);
}

bool BLEConnection::requestPHY(uint8_t phy)
{
  if (!_connected) return false;

  ble_gap_phys_t phys;
  phys.tx_phys = phy;
  phys.rx_phys = phy;
  return sd_ble_gap_phy_update(_conn_hdl, &phys) == NRF_SUCCESS;
}

bool BLEConnection::requestDataLengthUpdate(const ble_gap_data_length_params_t *params,
                                            ble_gap_data_length_limitation_t *limitation)
{
  if (!_connected) return false;
  return sd_ble_gap_data_length_update(_conn_hdl, params, limitation) == NRF_SUCCESS;
}

bool BLEConnection::requestMtuExchange(uint16_t mtu)
{
  if (!_connected) return false;

  /*
   * 우리가 감당할 수 있는 것보다 큰 값을 요구하지 않는다. 상대가 그대로 받아들이면
   * 우리 버퍼를 넘는 패킷이 들어오고, 그건 협상 실패보다 나쁘다.
   */
  uint16_t ours = sdAttMtu();
  if (mtu > ours) mtu = ours;

  return sd_ble_gattc_exchange_mtu_request(_conn_hdl, mtu) == NRF_SUCCESS;
}

bool BLEConnection::requestConnectionParameter(uint16_t conn_interval,
                                               uint16_t slave_latency,
                                               uint16_t sup_timeout)
{
  if (!_connected) return false;

  ble_gap_conn_params_t p;
  memset(&p, 0, sizeof(p));
  p.min_conn_interval = conn_interval;
  p.max_conn_interval = conn_interval;
  p.slave_latency     = slave_latency;
  p.conn_sup_timeout  = sup_timeout;

  return sd_ble_gap_conn_param_update(_conn_hdl, &p) == NRF_SUCCESS;
}

bool BLEConnection::disconnect(void)
{
  if (!_connected) return false;
  return sd_ble_gap_disconnect(_conn_hdl,
                               BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) == NRF_SUCCESS;
}
