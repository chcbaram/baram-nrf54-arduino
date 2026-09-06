/*
 * BLEConnection — 연결 하나에 대한 정보
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Adafruit Bluefruit52Lib 을 따른다.
 */
#ifndef _BLE_CONNECTION_H_
#define _BLE_CONNECTION_H_

#include <Arduino.h>
#include <ble.h>
#include <ble_gap.h>

class BLEConnection
{
  public:
    BLEConnection(void);

    /*
     * 수명은 세 단계다. Adafruit 이 new/delete 로 하는 것을 플래그로 옮긴 것이라
     * 순서가 같다 (R12).
     *   _begin()      연결됨.        Connection() 이 이 객체를 준다
     *   _disconnect() 끊김.          **disconnect 콜백 전에** 부른다. 그래야
     *                                콜백 안의 connected() 가 끊긴 링크를 빼고 센다.
     *                                객체는 아직 살아 있어 peer 주소를 읽을 수 있다
     *   _end()        슬롯 반납.     콜백이 끝난 뒤. 이후 Connection() 은 NULL
     */
    void _begin(const ble_evt_t *evt);
    void _disconnect(void);
    void _end(void);

    /** 슬롯이 쓰이고 있는가 (끊긴 직후 콜백 동안에도 true). */
    bool _inUse(void) const { return _in_use; }
    void _setMtu(uint16_t mtu) { _att_mtu = mtu; }

    uint16_t handle(void) const    { return _conn_hdl; }
    bool     connected(void) const { return _connected; }
    uint8_t  getRole(void) const   { return _role; }
    uint16_t getMtu(void) const    { return _att_mtu; }

    ble_gap_addr_t getPeerAddr(void) const { return _peer_addr; }

    /**
     * 상대 장치의 이름을 읽는다.
     *
     * ⚠ **아직 구현되지 않았다.** 상대의 GAP 서비스에서 Device Name 을
     *   읽어야 하는데, 그러려면 우리가 GATT **클라이언트**로 동작해야 한다
     *   (`sd_ble_gattc_*` + `BLE_CONN_CFG_GATTC`). 그 경로는 아직 없다.
     *
     *   지금은 **빈 문자열을 넣고 false 를 돌려준다.** 조용히 성공한 척하지
     *   않는다 — 호출한 쪽이 확인할 수 있어야 한다.
     *   Adafruit 예제는 반환값을 보지 않고 출력하므로 빈 줄이 찍힌다.
     *
     * @return 항상 false (미구현).
     */
    bool getPeerName(char *buf, uint16_t bufsize);

    bool disconnect(void);

    /**
     * RSSI 보고를 시작한다. 시작해야 getRssi() 와 Bluefruit.setRssiCallback()
     * 이 값을 받는다.
     * @param threshold_dbm 이만큼 바뀌어야 보고한다. 0 이면 매번.
     */
    bool monitorRssi(uint8_t threshold_dbm = 0);
    bool stopRssi(void);

    /** 마지막으로 보고된 RSSI. monitorRssi() 전에는 0 이다. */
    int8_t getRssi(void) const { return _rssi; }

    void _setRssi(int8_t v) { _rssi = v; }

  protected:
    uint16_t       _conn_hdl;
    bool           _in_use;
    bool           _connected;
    uint8_t        _role;
    uint16_t       _att_mtu;
    ble_gap_addr_t _peer_addr;
    int8_t         _rssi;
};

#endif
