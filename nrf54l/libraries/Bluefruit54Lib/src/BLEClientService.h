/*
 * BLEClientService / BLEClientCharacteristic — 상대의 서비스를 쓴다
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Adafruit Bluefruit52Lib 을 따른다 (CLAUDE.md R12 — 호환 우선).
 *
 * 우리가 GATT **클라이언트**로 상대의 서비스를 쓰는 쪽이다. 우리 GATT 서버에
 * 서비스를 올리는 BLEService / BLECharacteristic 과 짝을 이루되 방향이 반대다.
 *
 * ⚠ discover() 계열은 **블로킹**이다. 연결 콜백 안에서 부르는 것은 안전하다
 *   (콜백은 BLE 이벤트 태스크가 아니라 별도 태스크에서 돈다).
 *   스캔 콜백이나 이벤트 관찰자 안에서는 부르면 안 된다.
 */
#ifndef _BLE_CLIENT_SERVICE_H_
#define _BLE_CLIENT_SERVICE_H_

#include <Arduino.h>
#include <ble.h>
#include "BLEUuid.h"

/** 서비스 하나에 등록할 수 있는 characteristic 수. 고정 배열이다. */
#ifndef BLE_CLIENT_CHAR_MAX
#define BLE_CLIENT_CHAR_MAX   (8)
#endif

class BLEClientCharacteristic;

class BLEClientService
{
  public:
    BLEUuid uuid;

    BLEClientService(void);
    BLEClientService(BLEUuid bleuuid);

    /** 이벤트를 받도록 등록한다. begin() 하지 않으면 알림이 오지 않는다. */
    virtual bool begin(void);

    /** 상대에게서 이 서비스를 찾는다. 핸들 범위를 기억한다. */
    virtual bool discover(uint16_t conn_hdl);
    bool     discovered(void) const { return _conn_hdl != BLE_CONN_HANDLE_INVALID; }
    uint16_t connHandle(void) const { return _conn_hdl; }

    ble_gattc_handle_range_t getHandleRange(void) const { return _hdl_range; }
    void setHandleRange(ble_gattc_handle_range_t r)     { _hdl_range = r; }

    /**
     * 등록된 characteristic 을 **한 번의 탐색으로** 모두 채운다.
     *
     * ⚠ characteristic 마다 따로 탐색하면 서비스 범위를 그 수만큼 훑는다.
     *   DIS 처럼 특성이 여러 개면 눈에 띄게 느려진다. 한 번 훑고 UUID 로 나눠 준다.
     *
     * @return 짝을 찾은 characteristic 수.
     */
    uint8_t discoverCharacteristics(void);

    /* 코어 내부용 */
    bool _registerChar(BLEClientCharacteristic *chr);
    void _eventHandler(const ble_evt_t *evt);
    virtual void _disconnected(void);

    /** 마지막으로 begin() 된 서비스. 부모를 생략한 characteristic 이 이걸 쓴다. */
    static BLEClientService *lastService;

  protected:
    uint16_t _conn_hdl;
    ble_gattc_handle_range_t _hdl_range;

    BLEClientCharacteristic *_chars[BLE_CLIENT_CHAR_MAX];
    uint8_t _char_count;
};

#endif
