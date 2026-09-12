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

extern "C" {
#include "nrf_soc.h"
#include "utility/micro-ecc/uECC.h"
}

/*
 * ⚠ 바이트 순서가 이 파일의 유일한 함정이다.
 *   BLE 는 P-256 공개키를 {X, Y} 각각 **리틀엔디안**으로 주고받고
 *   (ble_gap.h 의 ble_gap_lesc_p256_pk_t 주석), DHKey 도 리틀엔디안이다.
 *   micro-ecc 는 **빅엔디안** 바이트 배열로 다룬다.
 *   그래서 32바이트 덩어리마다 뒤집는다. 안 뒤집으면 페어링이 그냥 실패하고,
 *   증상은 "LESC 만 안 된다" 로만 보인다.
 */
static void swap32(uint8_t *dst, const uint8_t *src)
{
  for (uint8_t i = 0; i < 32; i++) dst[i] = src[31 - i];
}

/*
 * uECC 가 쓸 난수. SoftDevice 가 켜져 있으면 그쪽 풀에서 받아야 한다.
 *
 * ⚠ S145 에는 nRF52 의 `sd_rand_application_bytes_available_get()` 이 **없다.**
 *   풀이 비면 `sd_rand_application_vector_get()` 이 NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES
 *   를 돌려주므로, 그때 잠깐 쉬었다 다시 청한다.
 */
static int lesc_rng(uint8_t *dest, unsigned size)
{
  while (size) {
    uint8_t chunk = (size > 32) ? 32 : (uint8_t) size;

    for (uint8_t retry = 0; ; retry++) {
      uint32_t err = sd_rand_application_vector_get(dest, chunk);
      if (err == NRF_SUCCESS) break;
      if (retry > 200) return 0;       /* 풀이 영영 안 차면 포기한다 */
      delay(2);
    }
    dest += chunk;
    size -= chunk;
  }
  return 1;
}

BLESecurity::BLESecurity(void)
{
  memset(&_sec_param, 0, sizeof(_sec_param));
  _sec_param.bond          = 1;
  _sec_param.mitm          = 0;
  _sec_param.lesc          = 1;   /* micro-ecc 로 지원한다 */
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

  _lesc_ready = false;
  memset(_lesc_priv, 0, sizeof(_lesc_priv));
  memset(&_lesc_own_pk, 0, sizeof(_lesc_own_pk));
  memset(&_lesc_peer_pk, 0, sizeof(_lesc_peer_pk));

  _passkey_cb     = NULL;
  _passkey_req_cb = NULL;
  _complete_cb    = NULL;
  _secured_cb     = NULL;
}

bool BLESecurity::begin(void)
{
  bondInit();

  /*
   * P-256 키쌍을 한 번 만든다. 공개키는 페어링마다 상대에게 넘어가고,
   * 개인키는 DHKey 계산에만 쓴다.
   *
   * ⚠ 이 계산은 수백 ms 걸릴 수 있다. Bluefruit.begin() 안에서 한 번이므로
   *   부팅이 그만큼 늦어진다 — 광고 시작 전에 끝난다.
   */
  uECC_set_rng(lesc_rng);

  uint8_t pub_be[64];
  uint8_t priv_be[32];

  if (!uECC_make_key(pub_be, priv_be, uECC_secp256r1())) {
    _lesc_ready = false;
    _sec_param.lesc = 0;      /* 못 만들었으면 레거시로 내려간다 */
    return false;
  }

  memcpy(_lesc_priv, priv_be, 32);
  swap32(&_lesc_own_pk.pk[0],  &pub_be[0]);    /* X */
  swap32(&_lesc_own_pk.pk[32], &pub_be[32]);   /* Y */
  _lesc_ready = true;
  return true;
}

void BLESecurity::setLESC(bool enabled)
{
  _sec_param.lesc = (enabled && _lesc_ready) ? 1 : 0;
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
       * LESC 를 쓰면 공개키 자리를 줘야 한다. SoftDevice 가 우리 것을 상대에게
       * 보내고, 상대 것을 여기에 받아 적는다.
       */
      if (_lesc_ready && _sec_param.lesc) {
        keyset.keys_own.p_pk  = &_lesc_own_pk;
        keyset.keys_peer.p_pk = &_lesc_peer_pk;
      }

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

    case BLE_GAP_EVT_LESC_DHKEY_REQUEST: {
      /*
       * 상대 공개키로 ECDH 를 계산해 돌려준다. keyset 으로 받아 둔
       * _lesc_peer_pk 가 이미 채워져 있다.
       *
       * ⚠ **답하지 않으면 페어링이 그대로 멈춘다.** 실패해도 반드시 응답한다 —
       *   그래야 상대가 실패를 알고 끝낸다.
       * ⚠ 이 계산은 수백 ms 걸린다. **BLE 이벤트 태스크에서 돌면 그동안 이벤트
       *   펌프가 멈춘다.** 지금은 이 핸들러가 이벤트 태스크에서 불리므로
       *   그만큼 지연이 생긴다 — 페어링 때 한 번뿐이라 받아들인다.
       */
      ble_gap_lesc_dhkey_t dhkey;
      memset(&dhkey, 0, sizeof(dhkey));

      bool ok = false;
      if (_lesc_ready) {
        uint8_t peer_be[64], secret_be[32];
        swap32(&peer_be[0],  &_lesc_peer_pk.pk[0]);    /* X */
        swap32(&peer_be[32], &_lesc_peer_pk.pk[32]);   /* Y */

        if (uECC_shared_secret(peer_be, _lesc_priv, secret_be, uECC_secp256r1())) {
          swap32(dhkey.key, secret_be);
          ok = true;
        }
      }
      /* ⚠ S145 는 sec_status 인자를 하나 더 받는다 (nRF52 의 S140 은 둘뿐이다). */
      sd_ble_gap_lesc_dhkey_reply(conn_hdl,
                                  ok ? BLE_GAP_SEC_STATUS_SUCCESS
                                     : BLE_GAP_SEC_STATUS_UNSPECIFIED,
                                  ok ? &dhkey : NULL);
      break;
    }

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

      /* LESC 로 맺었는지 스케치가 알 수 있게 남긴다 (진단용). */
      _last_lesc = st->lesc ? true : false;

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

    case BLE_GAP_EVT_CONN_SEC_UPDATE: {
      const ble_gap_conn_sec_t *sec = &evt->evt.gap_evt.params.conn_sec_update.conn_sec;

      /* level 2 이상이면 암호화된 링크다. CCCD 저장 여부가 여기 달렸다. */
      if (conn != NULL) conn->_setSecured(sec->sec_mode.lv >= 2);
      /* 콜백 태스크로 미룬다 — 그 안에서 탐색하는 스케치가 많다 (bluefruit.cpp). */
      if (_secured_cb) Bluefruit._deferSecured(conn_hdl);
      break;
    }

    /* CCCD 저장은 상대가 CCCD 를 쓰는 순간에 한다 (bluefruit.cpp 참조). */
    case BLE_GAP_EVT_DISCONNECTED:
      if (_pairing_conn_hdl == conn_hdl) _pairing_conn_hdl = BLE_CONN_HANDLE_INVALID;
      break;

    default:
      break;
  }
}
