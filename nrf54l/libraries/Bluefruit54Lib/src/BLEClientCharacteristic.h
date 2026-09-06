/*
 * BLEClientCharacteristic — 상대 서비스의 characteristic
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#ifndef _BLE_CLIENT_CHARACTERISTIC_H_
#define _BLE_CLIENT_CHARACTERISTIC_H_

#include <Arduino.h>
#include <ble.h>
#include "BLEUuid.h"

class BLEClientService;
class BLEClientCharacteristic;

class BLEClientCharacteristic
{
  public:
    typedef void (*notify_cb_t)(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len);

    BLEUuid uuid;

    BLEClientCharacteristic(void);
    BLEClientCharacteristic(BLEUuid bleuuid);

    /** 부모 서비스에 붙인다. NULL 이면 마지막으로 begin() 된 서비스에 붙는다. */
    void begin(BLEClientService *parent_svc = NULL);

    /** 부모 서비스 범위에서 이 UUID 를 찾는다. */
    bool discover(void);
    bool discovered(void) const { return _chr.handle_value != BLE_GATT_HANDLE_INVALID; }

    uint16_t connHandle(void) const;
    uint16_t valueHandle(void) const { return _chr.handle_value; }
    uint8_t  properties(void) const;

    BLEClientService *parentService(void) const { return _service; }

    /* ── 읽기 ────────────────────────────────────────────────────────── */
    uint16_t read(void *buffer, uint16_t bufsize);
    uint8_t  read8(void);
    uint16_t read16(void);
    uint32_t read32(void);

    /* ── 쓰기 (응답 없음) ────────────────────────────────────────────── */
    uint16_t write(const void *data, uint16_t len);
    uint16_t write8(uint8_t value);
    uint16_t write16(uint16_t value);
    uint16_t write32(uint32_t value);

    /* ── 쓰기 (응답 받음) ────────────────────────────────────────────── */
    uint16_t write_resp(const void *data, uint16_t len);
    uint16_t write8_resp(uint8_t value);
    uint16_t write16_resp(uint16_t value);
    uint16_t write32_resp(uint32_t value);

    /* ── 알림 ────────────────────────────────────────────────────────── */
    bool writeCCCD(uint16_t value);
    bool enableNotify(void);
    bool disableNotify(void);
    bool enableIndicate(void);
    bool disableIndicate(void);

    /**
     * 알림이 오면 불린다.
     *
     * ⚠ 이 콜백은 **BLE 이벤트 태스크에서 직접** 불린다. 안에서 블로킹 호출
     *   (read() / discover() 등)을 하면 안 된다 — 기다리는 응답을 처리할 주체가
     *   자기 자신이라 영영 안 온다.
     * @param use_ada_callback Adafruit 시그니처 호환. 우리는 항상 직접 부른다.
     */
    void setNotifyCallback(notify_cb_t fp, bool use_ada_callback = true);

    /* 코어 내부용 */
    void _assign(const ble_gattc_char_t *gattc_chr) { _chr = *gattc_chr; }
    void _setCccd(uint16_t hdl) { _cccd_handle = hdl; }
    void _eventHandler(const ble_evt_t *evt);
    void _disconnected(void);

  protected:
    ble_gattc_char_t  _chr;
    uint16_t          _cccd_handle;
    BLEClientService *_service;
    notify_cb_t       _notify_cb;
};

#endif
