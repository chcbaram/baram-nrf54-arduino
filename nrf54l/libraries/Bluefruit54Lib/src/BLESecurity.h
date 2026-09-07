/*
 * BLESecurity — 페어링과 본딩
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Adafruit Bluefruit52Lib 을 따른다 (CLAUDE.md R12 — 호환 우선).
 *
 * ⚠ **LESC(LE Secure Connections)는 아직 없다.** LESC 는 P-256 ECDH 가 필요한데,
 *   Adafruit 은 nRF52840 의 CryptoCell 로 한다. nRF54L 에는 CryptoCell 이 없고
 *   **CRACEN** 이 있어서 그 코드를 그대로 못 옮긴다.
 *   지금은 **레거시 페어링**만 한다 — 폰과의 본딩에는 충분하다.
 *   LESC 를 요구하는 상대와는 페어링이 실패하며, 조용히 넘어가지 않고
 *   pair complete 콜백에 실패 상태가 온다.
 */
#ifndef _BLE_SECURITY_H_
#define _BLE_SECURITY_H_

#include <Arduino.h>
#include <ble.h>
#include <ble_gap.h>
#include "bonding.h"

class BLESecurity
{
  public:
    /** 표시된 passkey 가 맞는지 사용자에게 묻는다. true 면 수락. */
    typedef bool (*pair_passkey_cb_t)(uint16_t conn_hdl, const uint8_t passkey[6], bool match_request);
    /** 상대가 passkey 를 입력하라고 요구한다. 6자리를 채워 준다. */
    typedef void (*pair_passkey_req_cb_t)(uint16_t conn_hdl, uint8_t passkey[6]);
    /** 페어링이 끝났다. auth_status 가 0(BLE_GAP_SEC_STATUS_SUCCESS)이면 성공. */
    typedef void (*pair_complete_cb_t)(uint16_t conn_hdl, uint8_t auth_status);
    /** 링크가 암호화됐다. */
    typedef void (*secured_conn_cb_t)(uint16_t conn_hdl);

    BLESecurity(void);

    bool begin(void);

    /**
     * 고정 PIN 6자리를 쓴다.
     *
     * ⚠ 고정 passkey 는 **레거시 + MITM + DisplayOnly** 조합에서만 쓸 수 있다.
     *   그래서 이 함수가 그 셋을 함께 설정한다. 뒤에 setIOCaps() 로 바꾸면
     *   PIN 이 안 먹는다.
     */
    bool setPIN(const char *pin);

    void setIOCaps(bool display, bool yes_no, bool keyboard);
    void setMITM(bool enabled);

    /** 이 연결에서 페어링을 시작한다 (central 이 거는 쪽). */
    bool authenticate(uint16_t conn_hdl);

    /** 저장된 키로 링크를 암호화한다 (central 재연결). */
    bool encrypt(uint16_t conn_hdl, const bond_keys_t *keys);

    void setPairPasskeyCallback(pair_passkey_cb_t fp)         { _passkey_cb = fp; }
    void setPairPasskeyRequestCallback(pair_passkey_req_cb_t fp) { _passkey_req_cb = fp; }
    void setPairCompleteCallback(pair_complete_cb_t fp)       { _complete_cb = fp; }
    void setSecuredCallback(secured_conn_cb_t fp)             { _secured_cb = fp; }

    /* 코어 내부용 */
    void _eventHandler(const ble_evt_t *evt);

  protected:
    ble_gap_sec_params_t _sec_param;

    /*
     * 페어링 중 주고받는 키를 담아 둘 자리. SoftDevice 가 **여기에 직접 쓰므로**
     * 페어링이 끝날 때까지 살아 있어야 한다.
     *
     * ⚠ 한 벌뿐이라 **동시에 두 링크와 페어링할 수 없다.** Adafruit 도 같다.
     *   실사용에서 페어링은 한 번에 하나씩 일어난다.
     */
    bond_keys_t _bond_keys;
    uint16_t    _pairing_conn_hdl;

    pair_passkey_cb_t     _passkey_cb;
    pair_passkey_req_cb_t _passkey_req_cb;
    pair_complete_cb_t    _complete_cb;
    secured_conn_cb_t     _secured_cb;
};

#endif
