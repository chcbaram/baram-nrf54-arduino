/*
 * BLECharacteristic — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 * API 는 Adafruit Bluefruit52Lib 을 따른다 (필요한 부분부터 채운다).
 */
#ifndef _BLE_CHARACTERISTIC_H_
#define _BLE_CHARACTERISTIC_H_

#include "BLEUuid.h"

/* Adafruit 관용 이름. CHR_PROPS_* 는 SoftDevice 비트와 1:1 이다. */
enum {
  CHR_PROPS_BROADCAST      = (1u << 0),
  CHR_PROPS_READ           = (1u << 1),
  CHR_PROPS_WRITE_WO_RESP  = (1u << 2),
  CHR_PROPS_WRITE          = (1u << 3),
  CHR_PROPS_NOTIFY         = (1u << 4),
  CHR_PROPS_INDICATE       = (1u << 5),
};

/*
 * 보안 모드. 값은 Adafruit 과 같고, `ble_gap_conn_sec_mode_t` 바이트를 그대로
 * 옮길 수 있게 만들어진 것이라 **0x<lv><sm>** 로 읽는다 (구현 주석 참조).
 */
typedef enum {
  SECMODE_NO_ACCESS        = 0x00,   /* sm=0 lv=0 */
  SECMODE_OPEN             = 0x11,   /* sm=1 lv=1 — 보안 없이 접근 */
  SECMODE_ENC_NO_MITM      = 0x21,   /* sm=1 lv=2 — 암호화 필요, MITM 불요 */
  SECMODE_ENC_WITH_MITM    = 0x31,   /* sm=1 lv=3 — 암호화 + MITM */
  SECMODE_SIGNED_NO_MITM   = 0x12,   /* sm=2 lv=1 */
  SECMODE_SIGNED_WITH_MITM = 0x22,   /* sm=2 lv=2 */
} BleSecurityMode;

/* notify 재시도 횟수와 한 번의 대기 한도. */
#define BLE_HVX_MAX_RETRY       (8)
#define BLE_HVX_TX_TIMEOUT_MS   (200)

class BLECharacteristic;
typedef void (*write_cb_t) (uint16_t conn_hdl, BLECharacteristic *chr, uint8_t *data, uint16_t len);

class BLECharacteristic
{
  public:
    BLEUuid uuid;

    BLECharacteristic(void);
    BLECharacteristic(uint16_t uuid16);
    BLECharacteristic(uint8_t const uuid128[16]);

    void setUuid(BLEUuid bleuuid);
    void setProperties(uint8_t prop);
    void setPermission(BleSecurityMode read, BleSecurityMode write);
    void setFixedLen(uint16_t len);
    void setMaxLen(uint16_t len);
    void setWriteCallback(write_cb_t fp);

    err_t begin(void);

    /** GATT 서버의 값을 갱신한다 (알림은 보내지 않는다). */
    uint16_t write(const void *data, uint16_t len);
    uint16_t write(const char *str);

    /**
     * 연결된 상대에게 notify 한다. 상대가 알림을 안 켰으면 값만 갱신하고 false.
     *
     * ⚠ 핸들 없는 판은 **모든 연결이 아니라 `Bluefruit.connHandle()` 한 곳으로만**
     *   간다 (Adafruit 과 같은 동작). 전체에 보내려면 스케치가 핸들을 돌아야 한다.
     */
    bool notify(uint16_t conn_hdl, const void *data, uint16_t len);
    bool notify(uint16_t conn_hdl, const char *str);
    bool notify(const void *data, uint16_t len);
    bool notify(const char *str);

    bool notifyEnabled(uint16_t conn_hdl) const;

    uint16_t handleValue(void) const { return _handles.value_handle; }
    uint16_t handleCccd(void)  const { return _handles.cccd_handle;  }

    /* 코어 내부용 — 이벤트 디스패치에서 쓴다. */
    void _eventHandler(const ble_evt_t *evt);

  protected:
    ble_gatts_char_handles_t _handles;
    uint8_t     _properties;
    uint16_t    _max_len;
    uint16_t    _fixed_len;
    uint8_t     _rd_sec;
    uint8_t     _wr_sec;
    write_cb_t  _wr_cb;
};

#endif
