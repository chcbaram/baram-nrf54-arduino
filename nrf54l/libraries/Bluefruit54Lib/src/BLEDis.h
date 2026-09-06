/*
 * BLEDis — Device Information Service (0x180A)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Adafruit Bluefruit52Lib 을 따른다.
 *
 * ⚠ 문자열은 **복사하지 않고 포인터만 들고 있다** (Adafruit 과 같다).
 *   문자열 리터럴이나 수명이 긴 버퍼를 넘겨라. 스택 변수를 넘기면
 *   begin() 뒤에 엉뚱한 값이 읽힌다.
 */
#ifndef _BLE_DIS_H_
#define _BLE_DIS_H_

#include "BLEService.h"
#include "BLECharacteristic.h"

#define BLE_DIS_CHAR_COUNT   6

class BLEDis : public BLEService
{
  public:
    BLEDis(void);

    void setManufacturer(const char *s) { _mfr  = s; }
    void setModel(const char *s)        { _model= s; }
    void setSerialNum(const char *s)    { _serial = s; }
    void setHardwareRev(const char *s)  { _hw   = s; }
    void setFirmwareRev(const char *s)  { _fw   = s; }
    void setSoftwareRev(const char *s)  { _sw   = s; }

    virtual err_t begin(void);

  protected:
    const char *_mfr, *_model, *_serial, *_hw, *_fw, *_sw;
    BLECharacteristic _chars[BLE_DIS_CHAR_COUNT];
};

#endif
