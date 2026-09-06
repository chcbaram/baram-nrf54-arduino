/*
 * BLEBas — Battery Service (0x180F)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#ifndef _BLE_BAS_H_
#define _BLE_BAS_H_

#include "BLEService.h"
#include "BLECharacteristic.h"

class BLEBas : public BLEService
{
  public:
    BLEBas(void);

    virtual err_t begin(void);

    /** 배터리 잔량 0~100 %. 알림이 켜져 있으면 함께 보낸다. */
    bool write(uint8_t percent);

    /** 마지막으로 쓴 값. */
    uint8_t read(void) const { return _level; }

  protected:
    BLECharacteristic _level_chr;
    uint8_t           _level;
};

#endif
