/*
 * BLEDfu — Nordic 레거시 DFU 서비스 (OTA 진입점)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * ⚠ **M3 시점에는 부트로더가 없다.** 서비스는 등록하지만 DFU 로 진입하지
 *   않는다. 제어 포인트에 쓰기가 오면 **레거시 DFU 프로토콜의 오류 응답을
 *   돌려준다** (NOT_SUPPORTED). 조용히 성공한 척하지 않는다 (CLAUDE.md §10 M3).
 *
 * 왜 굳이 두는가: Adafruit 예제 7개가 `bledfu.begin()` 을 부른다. 클래스가
 * 없으면 **컴파일이 실패한다** (R12 — 호환 우선).
 *
 * M4 에서 실제 트리거를 연결한다. 그때 바뀌는 것은 제어 포인트 핸들러 하나다.
 */
#ifndef _BLE_DFU_H_
#define _BLE_DFU_H_

#include "BLEService.h"
#include "BLECharacteristic.h"

class BLEDfu : public BLEService
{
  public:
    BLEDfu(void);

    virtual err_t begin(void);

    /** 제어 포인트에 쓰기가 몇 번 왔는지. 미지원 응답을 몇 번 보냈는지와 같다. */
    uint32_t requestCount(void) const { return _req_count; }

    /* 코어 내부용 */
    void _onControlWrite(void) { _req_count++; }

  protected:
    BLECharacteristic _chr_control;
    BLECharacteristic _chr_packet;
    BLECharacteristic _chr_revision;
    uint32_t          _req_count;
};

#endif
