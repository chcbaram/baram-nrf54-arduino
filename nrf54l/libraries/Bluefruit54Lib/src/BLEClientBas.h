/*
 * BLEClientBas — 상대의 배터리 서비스를 읽는다
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#ifndef _BLE_CLIENT_BAS_H_
#define _BLE_CLIENT_BAS_H_

#include "BLEClientService.h"
#include "BLEClientCharacteristic.h"

class BLEClientBas : public BLEClientService
{
  public:
    BLEClientBas(void);

    virtual bool begin(void);
    virtual bool discover(uint16_t conn_hdl);

    /** 배터리 잔량 (%). 못 읽으면 0. */
    uint8_t read(void);

    bool enableNotify(void)  { return _battery.enableNotify(); }
    bool disableNotify(void) { return _battery.disableNotify(); }

    void setNotifyCallback(BLEClientCharacteristic::notify_cb_t fp, bool use_ada_callback = true)
    {
      _battery.setNotifyCallback(fp, use_ada_callback);
    }

  protected:
    BLEClientCharacteristic _battery;
};

#endif
