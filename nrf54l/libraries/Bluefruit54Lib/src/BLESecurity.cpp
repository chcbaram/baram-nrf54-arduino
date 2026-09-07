/*
 * BLESecurity — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 페어링 흐름 (ble_gap.h 의 이벤트 순서):
 *   처음        연결 -> SEC_PARAMS_REQUEST -> (PASSKEY_DISPLAY / AUTH_KEY_REQUEST)
 *                    -> CONN_SEC_UPDATE -> AUTH_STATUS(키 저장)
 *   재연결      연결 -> SEC_INFO_REQUEST(저장된 키로 응답) -> CONN_SEC_UPDATE
 */
#include "bluefruit.h"
#include <string.h>

BLESecurity::BLESecurity(void)
{
  memset(&_sec_param, 0, sizeof(_sec_param));
  _sec_param.bond          = 1;
  _sec_param.mitm          = 0;
  _sec_param.lesc          = 0;   /* 아래 주석 — LESC 미지원 */
  _sec_param.keypress      = 0;
  _sec_param.io_caps       = BLE_GAP_IO_CAPS_NONE;
  _sec_param.oob           = 0;
  _sec_param.min_key_size  = 7;
  _sec_param.max_key_size  = 16;
  _sec_param.kdist_own.enc = 1;
  _sec_param.kdist_own.id  = 1;
  _sec_param.kdist_peer.enc = 1;
  _sec_param.kdist_peer.id  = 1;

  memset(&_bond_keys, 0, sizeof(_bond_keys));
  _pairing_conn_hdl = BLE_CONN_HANDLE_INVALID;

  _passkey_cb     = NULL;
  _passkey_req_cb = NULL;
  _complete_cb    = NULL;
  _secured_cb     = NULL;
}

bool BLESecurity::begin(void)
{
  bondInit();
  return true;
}

bool BLESecurity::setPIN(const char *pin)
{
  if (pin == NULL || strlen(pin) != BLE_GAP_PASSKEY_LEN) return false;

  /* 고정 passkey 는 이 조합에서만 유효하다 (헤더 주석 참조). */
  _sec_param.mitm    = 1;
  _sec_param.lesc    = 0;
  _sec_param.io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY;

  ble_opt_t opt;
  memset(&opt, 0, sizeof(opt));
  opt.gap_opt.passkey.p_passkey = (const uint8_t *) pin;

  return sd_ble_opt_set(BLE_GAP_OPT_PASSKEY, &opt) == NRF_SUCCESS;
}

void BLESecurity::setIOCaps(bool display, bool yes_no, bool keyboard)
{
  uint8_t caps = BLE_GAP_IO_CAPS_NONE;

  if (display) {
    if (keyboard)     caps = BLE_GAP_IO_CAPS_KEYBOARD_DISPLAY;
    else if (yes_no)  caps = BLE_GAP_IO_CAPS_DISPLAY_YESNO;
    else              caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY;
  } else if (keyboard) {
    caps = BLE_GAP_IO_CAPS_KEYBOARD_ONLY;
  }
  _sec_param.io_caps = caps;
}

void BLESecurity::setMITM(bool enabled)
{
  _sec_param.mitm = enabled ? 1 : 0;
}

bool BLESecurity::authenticate(uint16_t conn_hdl)
{
  return sd_ble_gap_authenticate(conn_hdl, &_sec_param) == NRF_SUCCESS;
}

bool BLESecurity::encrypt(uint16_t conn_hdl, const bond_keys_t *keys)
{
  if (keys == NULL) return false;
  return sd_ble_gap_encrypt(conn_hdl, &keys->peer_enc.master_id,
                            &keys->peer_enc.enc_info) == NRF_SUCCESS;
}

void BLESecurity::_eventHandler(const ble_evt_t *evt)
{
  const uint16_t conn_hdl = evt->evt.gap_evt.conn_handle;
  BLEConnection *conn     = Bluefruit.Connection(conn_hdl);

  switch (evt->header.evt_id) {
    case BLE_GAP_EVT_SEC_PARAMS_REQUEST: {
      /*
       * 상대가 우리 보안 파라미터를 묻는다. 여기서 키를 담을 자리도 함께 준다 —
       * SoftDevice 가 페어링이 끝날 때까지 **그 주소에 직접 쓴다.**
       */
      _pairing_conn_hdl = conn_hdl;
      memset(&_bond_keys, 0, sizeof(_bond_keys));

      ble_gap_sec_keyset_t keyset;
      memset(&keyset, 0, sizeof(keyset));
      keyset.keys_own.p_enc_key   = &_bond_keys.own_enc;
      keyset.keys_peer.p_enc_key  = &_bond_keys.peer_enc;
      keyset.keys_peer.p_id_key   = &_bond_keys.peer_id;

      /*
       * ⚠ peripheral 일 때만 우리 파라미터를 보낸다. central 은 이미
       *   authenticate() 에서 냈으므로 NULL 을 줘야 한다 (ble_gap.h 규정).
       */
      const bool periph = (conn != NULL) && (conn->getRole() == BLE_GAP_ROLE_PERIPH);

      sd_ble_gap_sec_params_reply(conn_hdl, BLE_GAP_SEC_STATUS_SUCCESS,
                                  periph ? &_sec_param : NULL, &keyset);
      break;
    }

    case BLE_GAP_EVT_PASSKEY_DISPLAY: {
      const ble_gap_evt_passkey_display_t *pd = &evt->evt.gap_evt.params.passkey_display;

      /*
       * ⚠ match_request 면 **반드시 답해야 한다.** 안 답하면 페어링이 그대로
       *   멈춘다. 콜백이 없으면 수락으로 본다 — 표시할 화면이 없는 보드가 흔하다.
       */
      bool accept = true;
      if (_passkey_cb) accept = _passkey_cb(conn_hdl, pd->passkey, pd->match_request);

      if (pd->match_request) {
        sd_ble_gap_auth_key_reply(conn_hdl,
                                  accept ? BLE_GAP_AUTH_KEY_TYPE_PASSKEY
                                         : BLE_GAP_AUTH_KEY_TYPE_NONE, NULL);
      }
      break;
    }

    case BLE_GAP_EVT_AUTH_KEY_REQUEST: {
      /* 상대가 passkey 입력을 요구한다. 스케치가 못 주면 거절해야 진행이 끝난다. */
      uint8_t passkey[BLE_GAP_PASSKEY_LEN] = { 0 };

      if (_passkey_req_cb) {
        _passkey_req_cb(conn_hdl, passkey);
        sd_ble_gap_auth_key_reply(conn_hdl, BLE_GAP_AUTH_KEY_TYPE_PASSKEY, passkey);
      } else {
        sd_ble_gap_auth_key_reply(conn_hdl, BLE_GAP_AUTH_KEY_TYPE_NONE, NULL);
      }
      break;
    }

    /*
     * ⚠ LESC 는 아직 못 한다 (P-256 ECDH — nRF54L 은 CRACEN 이라 Adafruit 의
     *   CryptoCell 코드를 못 옮긴다). 여기까지 왔다는 건 상대가 LESC 를 골랐다는
     *   뜻인데, 답할 키가 없으므로 **명확히 끊는다.** 응답을 안 하면 상대가
     *   타임아웃까지 기다려 원인이 안 보인다.
     */
    case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
      sd_ble_gap_disconnect(conn_hdl, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      if (_complete_cb) _complete_cb(conn_hdl, BLE_GAP_SEC_STATUS_AUTH_REQ);
      break;

    case BLE_GAP_EVT_AUTH_STATUS: {
      const ble_gap_evt_auth_status_t *st = &evt->evt.gap_evt.params.auth_status;

      if (st->auth_status == BLE_GAP_SEC_STATUS_SUCCESS && st->bonded) {
        /*
         * ⚠ 상대가 IRK 를 안 줄 수 있다 (공개/정적 주소를 쓰는 기기). 그러면
         *   저장된 identity 주소가 비어 재연결 때 못 찾는다. 연결 주소를 대신 넣는다.
         */
        if (!st->kdist_peer.id && conn != NULL) {
          _bond_keys.peer_id.id_addr_info = conn->getPeerAddr();
        }
        uint8_t role = (conn != NULL) ? conn->getRole() : BLE_GAP_ROLE_PERIPH;
        bondSaveKeys(role, &_bond_keys);
      }

      _pairing_conn_hdl = BLE_CONN_HANDLE_INVALID;
      if (_complete_cb) _complete_cb(conn_hdl, st->auth_status);
      break;
    }

    case BLE_GAP_EVT_SEC_INFO_REQUEST: {
      /* 재연결이다. 저장된 키가 있으면 그것으로 답하고, 없으면 NULL 로 답해
       * 상대가 다시 페어링을 걸게 한다. */
      const ble_gap_evt_sec_info_request_t *req = &evt->evt.gap_evt.params.sec_info_request;
      uint8_t role = (conn != NULL) ? conn->getRole() : BLE_GAP_ROLE_PERIPH;

      /*
       * ⚠ S145 의 이 함수는 인자가 **둘뿐이다** — nRF52 의 S140 은
       *   (conn, enc, id, sign) 넷을 받는다. Adafruit 코드를 그대로 옮기면
       *   컴파일이 막힌다. IRK 는 여기서 넘기지 않는다.
       */
      bond_keys_t keys;
      if (bondLoadKeys(role, &req->peer_addr, &keys)) {
        sd_ble_gap_sec_info_reply(conn_hdl, &keys.own_enc.enc_info);
      } else {
        sd_ble_gap_sec_info_reply(conn_hdl, NULL);
      }
      break;
    }

    case BLE_GAP_EVT_CONN_SEC_UPDATE:
      if (_secured_cb) _secured_cb(conn_hdl);
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      /*
       * 암호화된 링크였다면 CCCD 를 남겨 둔다. 안 남기면 재연결 때 상대가
       * 알림을 다시 켜야 하고, 본딩의 의미가 반쯤 사라진다.
       */
      if (conn != NULL) {
        ble_gap_addr_t peer = conn->getPeerAddr();
        bondSaveCccd(conn->getRole(), conn_hdl, &peer);
      }
      if (_pairing_conn_hdl == conn_hdl) _pairing_conn_hdl = BLE_CONN_HANDLE_INVALID;
      break;

    default:
      break;
  }
}
