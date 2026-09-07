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
     * 상대 장치의 이름을 읽는다 (상대 GAP 서비스의 Device Name).
     *
     * 우리가 peripheral 이어도 읽을 수 있다 — GATT 의 클라이언트/서버는
     * 연결의 central/peripheral 역할과 별개다.
     *
     * ⚠ **블로킹이다.** 상대의 응답을 기다린다. 연결 콜백 안에서 부르는 것은
     *   안전하다 (콜백은 BLE 이벤트 태스크가 아니라 별도 태스크에서 돈다).
     *   ISR 이나 이벤트 관찰자 안에서는 부르면 안 된다.
     *
     * ⚠ 상대가 이름을 공개하지 않으면 0 이다. 흔한 일이다 —
     *   iOS 는 본딩 전에는 GAP Device Name 을 안 준다.
     *
     * @return 읽은 바이트 수 (널 종료 포함하지 않음). 실패하면 0.
     */
    uint16_t getPeerName(char *buf, uint16_t bufsize);

    /** 이 상대와 본딩해 둔 키가 있는가. */
    bool bonded(void);

    /** 페어링을 시작한다. central 이 거는 쪽이다. */
    bool requestPairing(void);

    bool disconnect(void);

    /**
     * PHY 를 바꾸자고 **요청**한다. 기본값 AUTO 는 양쪽이 함께 지원하는 가장 빠른
     * 것을 SoftDevice 가 고르게 한다 — 보통 2M PHY 로 올라가고 처리량이 대략
     * 두 배가 된다.
     *
     * ⚠ 요청일 뿐이다. 상대가 거절하거나 2M 을 지원하지 않으면 1M 으로 남는다.
     *   **성공 여부가 아니라 요청을 보냈는지만 돌려준다.** 실제 결과는
     *   `BLE_GAP_EVT_PHY_UPDATE` 로 오고, 그때 getPHY() 가 갱신된다.
     */
    bool requestPHY(uint8_t phy = BLE_GAP_PHY_AUTO);

    /** 마지막으로 확정된 TX PHY (`BLE_GAP_PHY_1MBPS` 등). 협상 전에는 1M 이다. */
    uint8_t getPHY(void) const { return _phy; }

    /**
     * 현재 연결 간격. 단위는 1.25 ms 라 12 이면 15 ms 다.
     *
     * 처리량을 볼 때 MTU 나 PHY 보다 이게 먼저다 — 한 연결 이벤트에 몇 패킷이
     * 실리느냐를 정하는 것이 이 값이고, **정하는 쪽은 central 이다.**
     */
    uint16_t getConnectionInterval(void) const { return _conn_interval; }
    void _setConnParams(const ble_gap_conn_params_t *p);

    /**
     * 링크 계층 데이터 길이(DLE)를 늘리자고 요청한다. NULL 을 넘기면 SoftDevice 가
     * 최대치를 고른다 — 거의 항상 그게 맞다.
     *
     * MTU 만 키우고 이걸 빠뜨리면 큰 ATT 패킷이 여러 링크 계층 프레임으로 쪼개져
     * 처리량이 생각만큼 오르지 않는다. 둘은 같이 가야 한다.
     */
    bool requestDataLengthUpdate(const ble_gap_data_length_params_t *params = NULL,
                                 ble_gap_data_length_limitation_t *limitation = NULL);

    /**
     * ATT MTU 협상을 **우리가 먼저** 건다 (GATT 클라이언트 역할).
     *
     * ⚠ GATT 의 클라이언트/서버는 연결의 central/peripheral 과 별개라,
     *   peripheral 이어도 이걸 부를 수 있다. 상류 throughput 예제가 그렇게 한다.
     * ⚠ 비동기다. 협상된 값은 응답이 온 뒤 getMtu() 에 반영된다.
     */
    bool requestMtuExchange(uint16_t mtu);

    /**
     * 연결 간격 등을 바꾸자고 요청한다. 단위는 1.25 ms 라 6 이 7.5 ms 다.
     *
     * ⚠ 상대가 거절할 수 있다. iOS 는 특히 까다롭다 —
     *   Apple 의 Accessory Design Guidelines 범위를 벗어나면 무시된다.
     * @param sup_timeout 10 ms 단위. 기본 2 초 (상류와 같은 값).
     */
    bool requestConnectionParameter(uint16_t conn_interval,
                                    uint16_t slave_latency = 0,
                                    uint16_t sup_timeout   = 200);

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

    /** 링크가 암호화됐는가 (security level 2 이상). */
    bool secured(void) const { return _secured; }
    void _setSecured(bool v) { _secured = v; }
    void _setPhy(uint8_t v)  { _phy = v; }

  protected:
    uint16_t       _conn_hdl;
    bool           _in_use;
    bool           _connected;
    bool           _secured;
    uint8_t        _role;
    uint16_t       _att_mtu;
    ble_gap_addr_t _peer_addr;
    int8_t         _rssi;
    uint8_t        _phy;
    uint16_t       _conn_interval;
    uint16_t       _slave_latency;
    uint16_t       _sup_timeout;
};

#endif
