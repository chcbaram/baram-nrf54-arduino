/*
 * BLEClientDis — 상대의 장치 정보 서비스를 읽는다
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#ifndef _BLE_CLIENT_DIS_H_
#define _BLE_CLIENT_DIS_H_

#include "BLEClientService.h"

class BLEClientDis : public BLEClientService
{
  public:
    BLEClientDis(void);

    virtual bool begin(void);
    virtual bool discover(uint16_t conn_hdl);

    /**
     * DIS characteristic 하나를 문자열로 읽는다. 널을 붙여 준다.
     *
     * ⚠ characteristic 을 미리 등록해 두지 않고 **부를 때마다 찾아 읽는다.**
     *   DIS 는 특성이 여섯이나 되는데 보통 한둘만 쓰므로, 여섯 개를 상주시키는
     *   것보다 그때그때 찾는 편이 낫다 (Adafruit 도 같은 이유로 그렇게 한다).
     *
     * @return 읽은 바이트 수 (널 제외). 못 찾거나 실패하면 0.
     */
    uint16_t getChars(uint16_t uuid16, char *buffer, uint16_t bufsize);

    uint16_t getModel(char *buf, uint16_t n)        { return getChars(UUID16_CHR_MODEL_NUMBER_STRING, buf, n); }
    uint16_t getSerial(char *buf, uint16_t n)       { return getChars(UUID16_CHR_SERIAL_NUMBER_STRING, buf, n); }
    uint16_t getFirmwareRev(char *buf, uint16_t n)  { return getChars(UUID16_CHR_FIRMWARE_REVISION_STRING, buf, n); }
    uint16_t getHardwareRev(char *buf, uint16_t n)  { return getChars(UUID16_CHR_HARDWARE_REVISION_STRING, buf, n); }
    uint16_t getSoftwareRev(char *buf, uint16_t n)  { return getChars(UUID16_CHR_SOFTWARE_REVISION_STRING, buf, n); }
    uint16_t getManufacturer(char *buf, uint16_t n) { return getChars(UUID16_CHR_MANUFACTURER_NAME_STRING, buf, n); }
};

#endif
