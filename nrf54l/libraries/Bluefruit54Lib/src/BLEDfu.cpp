/*
 * BLEDfu — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * UUID 는 Nordic 레거시 DFU 서비스 (00001530-1212-EFDE-1523-785FEABCD123).
 * Adafruit Bluefruit52Lib 의 BLEDfu 와 같은 값이라 nRF Connect / Bluefruit LE
 * 앱이 그대로 인식한다.
 */
#include "BLEDfu.h"
#include "bluefruit.h"

/* little-endian 배열 */
static const uint8_t UUID_SVC_DFU[16] = {
  0x23,0xD1,0xBC,0xEA,0x5F,0x78,0x23,0x15, 0xDE,0xEF,0x12,0x12,0x30,0x15,0x00,0x00
};
static const uint8_t UUID_CHR_CONTROL[16] = {
  0x23,0xD1,0xBC,0xEA,0x5F,0x78,0x23,0x15, 0xDE,0xEF,0x12,0x12,0x31,0x15,0x00,0x00
};
static const uint8_t UUID_CHR_PACKET[16] = {
  0x23,0xD1,0xBC,0xEA,0x5F,0x78,0x23,0x15, 0xDE,0xEF,0x12,0x12,0x32,0x15,0x00,0x00
};
static const uint8_t UUID_CHR_REVISION[16] = {
  0x23,0xD1,0xBC,0xEA,0x5F,0x78,0x23,0x15, 0xDE,0xEF,0x12,0x12,0x34,0x15,0x00,0x00
};

/* 레거시 DFU 제어 포인트 응답: [0x10, 요청 opcode, 상태] */
#define DFU_OP_RESPONSE            0x10
#define DFU_STATUS_NOT_SUPPORTED   0x03

/* 앱 모드임을 알리는 리비전 값 (Adafruit 과 동일). */
#define DFU_REV_APPMODE            0x0001

static BLEDfu *_dfu_instance = NULL;

static void bledfu_control_wr_cb(uint16_t conn_hdl, BLECharacteristic *chr,
                                 uint8_t *data, uint16_t len)
{
  (void) conn_hdl;

  if (_dfu_instance == NULL || len == 0) return;

  /*
   * 부트로더가 없으므로 **무엇을 요청하든 미지원으로 답한다.**
   * 여기서 조용히 무시하면 상대(nRF Connect 등)가 응답을 기다리다 타임아웃 나고,
   * 사용자는 "DFU 가 되는 줄 알았는데 멈췄다" 로 읽는다.
   */
  _dfu_instance->_onControlWrite();

  uint8_t rsp[3] = { DFU_OP_RESPONSE, data[0], DFU_STATUS_NOT_SUPPORTED };
  chr->notify(rsp, sizeof(rsp));
}

BLEDfu::BLEDfu(void)
  : BLEService(UUID_SVC_DFU),
    _chr_control(UUID_CHR_CONTROL),
    _chr_packet(UUID_CHR_PACKET),
    _chr_revision(UUID_CHR_REVISION)
{
  _req_count = 0;
}

err_t BLEDfu::begin(void)
{
  _dfu_instance = this;

  err_t err = BLEService::begin();
  if (err) return err;

  /*
   * ⚠ packet / revision characteristic 을 지역 변수로 만들면 안 된다.
   *   BLECharacteristic 은 쓰기 이벤트를 받으려고 자기 주소를 Bluefruit 에
   *   등록하므로, 스택에 두면 begin() 이 끝나는 순간 매달린 포인터가 된다.
   *   Adafruit 은 setTempMemory() 로 그 등록을 건너뛰지만 우리는 멤버로 둔다.
   */
  _chr_packet.setProperties(CHR_PROPS_WRITE_WO_RESP);
  _chr_packet.setMaxLen(20);
  err = _chr_packet.begin();
  if (err) return err;

  _chr_control.setProperties(CHR_PROPS_WRITE | CHR_PROPS_NOTIFY);
  _chr_control.setMaxLen(23);
  _chr_control.setWriteCallback(bledfu_control_wr_cb);
  err = _chr_control.begin();
  if (err) return err;

  _chr_revision.setProperties(CHR_PROPS_READ);
  _chr_revision.setFixedLen(2);
  err = _chr_revision.begin();
  if (err) return err;

  uint16_t rev = DFU_REV_APPMODE;
  _chr_revision.write(&rev, 2);

  return NRF_SUCCESS;
}
