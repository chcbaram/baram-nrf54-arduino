/*
 * BLEClientCts — 상대의 Current Time Service(0x1805)를 읽는다
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 보드가 central 로 붙어 **상대의 시계를 읽는** 쪽이다. 보통 상대는 폰이다 —
 * iOS 는 CTS 서버를 기본으로 띄우므로 연결만 하면 시각을 가져올 수 있다.
 *
 * API 는 상류를 따른다 (R12). 읽은 값은 `Time` / `LocalInfo` 에 남는다.
 */
#ifndef _BLE_CLIENT_CTS_H_
#define _BLE_CLIENT_CTS_H_

#include "BLEClientService.h"
#include "BLEClientCharacteristic.h"

class BLEClientCts : public BLEClientService
{
  public:
    /** 시각이 바뀐 이유 비트. `Time.adjust_reason` 을 그대로 넘긴다. */
    typedef void (*adjust_callback_t)(uint8_t reason);

    BLEClientCts(void);

    virtual bool begin(void);
    virtual bool discover(uint16_t conn_hdl);

    /** 상대의 현재 시각을 읽어 `Time` 에 채운다. */
    bool getCurrentTime(void);

    /** 시간대 / 서머타임을 읽어 `LocalInfo` 에 채운다. 없는 상대도 있다. */
    bool getLocalTimeInfo(void);

    /**
     * 상대가 시각을 고칠 때 알림을 받는다.
     * ⚠ 알림이 오면 `Time` 이 **자동으로 갱신된다.** 콜백에서 다시 읽을 필요가 없다.
     */
    bool enableAdjust(void);
    void setAdjustCallback(adjust_callback_t fp);

    /**
     * Current Time (0x2A2B). 규격 그대로의 배치라 읽은 바이트를 그대로 얹는다.
     * `weekday` 는 1 = 월요일, 0 = 모름. `subsecond` 는 1/256 초 단위다.
     */
    struct __attribute__((packed)) {
      uint16_t year;
      uint8_t  month;
      uint8_t  day;
      uint8_t  hour;
      uint8_t  minute;
      uint8_t  second;
      uint8_t  weekday;
      uint8_t  subsecond;
      uint8_t  adjust_reason;
    } Time;

    /** Local Time Information (0x2A0F). timezone 은 15분 단위다 (예: 서울 = 36). */
    struct __attribute__((packed)) {
      int8_t  timezone;
      uint8_t dst_offset;
    } LocalInfo;

    /* 내부용 — 정적 알림 콜백이 부른다. */
    void _curTimeNotify(uint8_t *data, uint16_t len);

  protected:
    BLEClientCharacteristic _cur_time;
    BLEClientCharacteristic _local_info;

    adjust_callback_t _adjust_cb;
};

#endif
