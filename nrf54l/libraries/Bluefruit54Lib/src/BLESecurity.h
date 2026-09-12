/*
 * BLESecurity — 페어링과 본딩
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Adafruit Bluefruit52Lib 을 따른다 (CLAUDE.md R12 — 호환 우선).
 *
 * LESC(LE Secure Connections)는 **micro-ecc 소프트웨어 P-256** 으로 한다.
 *
 * ⚠ nRF54L 의 CRACEN 하드웨어 가속기는 못 쓴다 — `nrfx_cracen.h` 가 난수만
 *   내주고 ECC 는 NCS 의 nrf_security(PSA Crypto)를 거쳐야 닿는데, Arduino
 *   코어에 끌어오기엔 너무 크다. Adafruit 이 nRF52840 에서 쓰는 CryptoCell
 *   경로도 이 칩엔 없다. Nordic 자신이 CryptoCell 없는 nRF52832 에서 쓴 방법이
 *   micro-ecc 다.
 *
 * ⚠ 키쌍은 `begin()` 에서 한 번 만든다. 페어링마다 ECDH 를 한 번 계산하며
 *   그 동안 그 태스크가 수백 ms 잡힐 수 있다 — 콜백 태스크에서 돈다.
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

    /**
     * LESC 사용 여부. 기본은 켜져 있다.
     *
     * ⚠ 끄면 레거시 페어링이 되는데, 레거시는 **페어링 순간을 도청당하면
     *   LTK 가 유도된다.** 상대가 LESC 를 못 하는 경우에만 꺼라.
     */
    void setLESC(bool enabled);

    /** 마지막 페어링이 LESC 였는가. 페어링 완료 콜백 안에서 읽는다. */
    bool lastPairingWasLesc(void) const { return _last_lesc; }

    /** 이 연결에서 페어링을 시작한다 (central 이 거는 쪽). */
    bool authenticate(uint16_t conn_hdl);

    /** 저장된 키로 링크를 암호화한다 (central 재연결). */
    bool encrypt(uint16_t conn_hdl, const bond_keys_t *keys);

    void setPairPasskeyCallback(pair_passkey_cb_t fp)         { _passkey_cb = fp; }
    void setPairPasskeyRequestCallback(pair_passkey_req_cb_t fp) { _passkey_req_cb = fp; }
    void setPairCompleteCallback(pair_complete_cb_t fp)       { _complete_cb = fp; }
    void setSecuredCallback(secured_conn_cb_t fp)             { _secured_cb = fp; }

    /* 내부용 — 콜백 태스크가 부른다 (bluefruit.cpp 의 BLE_CB_SECURED). */
    void _invokeSecuredCallback(uint16_t conn_hdl)
    {
      if (_secured_cb) _secured_cb(conn_hdl);
    }

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

    /*
     * P-256 키쌍. 공개키는 SoftDevice 가 keyset 으로 들고 있으므로 살아 있어야 한다.
     * 바이트 순서: uECC 는 빅엔디안, BLE 는 리틀엔디안이다 (구현 주석 참조).
     */
    bool     _lesc_ready;
    bool     _last_lesc = false;
    uint8_t  _lesc_priv[32];
    ble_gap_lesc_p256_pk_t _lesc_own_pk;    /* 리틀엔디안 — SoftDevice 에 넘기는 형식 */
    ble_gap_lesc_p256_pk_t _lesc_peer_pk;

    pair_passkey_cb_t     _passkey_cb;
    pair_passkey_req_cb_t _passkey_req_cb;
    pair_complete_cb_t    _complete_cb;
    secured_conn_cb_t     _secured_cb;
};

#endif
