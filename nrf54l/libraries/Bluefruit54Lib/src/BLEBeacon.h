/*
 * BLEBeacon — iBeacon 광고 페이로드
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Adafruit Bluefruit52Lib 을 따른다 (CLAUDE.md R12 — 호환 우선).
 *
 * 서비스가 아니라 **광고 페이로드 생성기**다. GATT 를 쓰지 않고 광고만 한다.
 * 그래서 연결도 필요 없다 — 예제는 non-connectable 로 광고한다.
 */
#ifndef _BLE_BEACON_H_
#define _BLE_BEACON_H_

#include <Arduino.h>
#include "BLEUuid.h"

class BLEAdvertising;

class BLEBeacon
{
  public:
    BLEBeacon(void);
    BLEBeacon(uint8_t const uuid128[16]);
    BLEBeacon(uint8_t const uuid128[16], uint16_t major, uint16_t minor, int8_t rssi);

    /**
     * 제조사 ID. iBeacon 은 **제조사 고유 데이터** 필드를 쓰므로 반드시 있어야 한다.
     * 기본값은 Apple(0x004C) 이다 — 원본 규격이 그렇다.
     */
    void setManufacturer(uint16_t manufacturer);
    void setUuid(uint8_t const uuid128[16]);
    void setMajorMinor(uint16_t major, uint16_t minor);

    /** 1 m 거리에서 관측되는 RSSI. 수신 측이 거리를 추정하는 기준값이다. */
    void setRssiAt1m(int8_t rssi);

    bool start(void);
    bool start(BLEAdvertising &adv);

  protected:
    uint16_t       _manufacturer_id;
    uint8_t const *_uuid128;
    /*
     * ⚠ iBeacon 의 major/minor 는 **빅엔디안**이다. BLE 광고의 나머지가 전부
     *   리틀엔디안이라 그냥 넣기 쉬운데, 그러면 스캐너가 값을 뒤집어 읽는다.
     *   넣을 때 한 번 뒤집어 보관한다.
     */
    uint16_t       _major_be;
    uint16_t       _minor_be;
    int8_t         _rssi_at_1m;

    void _init(void);
};

/* ── EddyStone URL ─────────────────────────────────────────────────── */

enum {
  EDDYSTONE_TYPE_UID = 0x00,
  EDDYSTONE_TYPE_URL = 0x10,
  EDDYSTONE_TYPE_TLM = 0x20,
  EDDYSTONE_TYPE_EID = 0x30,
};

/**
 * EddyStone URL 프레임.
 *
 * URL 을 그대로 싣지 않고 **접두사와 흔한 꼬리를 1바이트 코드로 압축**한다
 * ("https://www." -> 0x01, ".com/" -> 0x00). 광고 패킷이 31바이트뿐이라
 * 규격이 그렇게 정해져 있다. 압축 후 17바이트를 넘으면 start() 가 false 다.
 */
class EddyStoneUrl
{
  public:
    EddyStoneUrl(void);
    EddyStoneUrl(int8_t rssi_at_0m, const char *url = NULL);

    void setUrl(const char *url);
    void setRssi(int8_t rssi_at_0m);

    bool start(void);

  protected:
    int8_t      _rssi;
    const char *_url;
};

#endif
