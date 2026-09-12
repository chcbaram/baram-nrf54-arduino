/*
 * BLEMidi — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>
#include <stdlib.h>

/* BLE-MIDI 1.0 규격 UUID. 리틀엔디안 바이트열이다. */
const uint8_t BLEMIDI_UUID_SERVICE[] =
{
  0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7,
  0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03
};

const uint8_t BLEMIDI_UUID_CHR_IO[] =
{
  0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1,
  0x12, 0x41, 0x68, 0x38, 0xDB, 0xE5, 0x72, 0x77
};

/*------------------------------------------------------------------*/
/* BLE-MIDI 패킷                                                     */
/*------------------------------------------------------------------*/
/*
 * 규격상 모든 패킷은 헤더로 시작하고, 그 뒤에 [타임스탬프 + MIDI 바이트] 가 온다.
 * 헤더와 타임스탬프는 **둘 다 bit7 이 1** 이다 — 그래서 수신 파서가
 * "bit7 이 선 바이트" 를 타임스탬프로 보고 건너뛴다.
 *
 * 타임스탬프는 13비트 밀리초다: 상위 6비트가 헤더에, 하위 7비트가 다음 바이트에.
 */
typedef union __attribute__((packed)) {
  struct {
    uint8_t timestamp_hi : 6;
    uint8_t              : 1;
    uint8_t start_bit    : 1;
  };
  uint8_t byte;
} midi_header_t;

typedef union __attribute__((packed)) {
  struct {
    uint8_t timestamp_low : 7;
    uint8_t start_bit     : 1;
  };
  uint8_t byte;
} midi_timestamp_t;

typedef struct __attribute__((packed)) {
  midi_header_t    header;
  midi_timestamp_t timestamp;
  uint8_t          data[BLE_MIDI_TX_BUFFER_SIZE];
} midi_event_packet_t;

typedef struct __attribute__((packed)) {
  midi_header_t header;
  uint8_t       data[BLE_MIDI_TX_BUFFER_SIZE];
} midi_split_packet_t;

static_assert(sizeof(midi_header_t) == 1, "header must be one byte");
static_assert(sizeof(midi_timestamp_t) == 1, "timestamp must be one byte");
static_assert(sizeof(midi_event_packet_t) == BLE_MIDI_TX_BUFFER_SIZE + 2, "packet layout");
static_assert(sizeof(midi_split_packet_t) == BLE_MIDI_TX_BUFFER_SIZE + 1, "packet layout");

/*------------------------------------------------------------------*/
/* 정적 콜백 — BLEUart 과 같은 방식이다.                              */
/* (우리 BLECharacteristic 에는 parentService() 가 없다.)             */
/*------------------------------------------------------------------*/
static BLEMidi *_midi_instance = NULL;

static void blemidi_write_cb(uint16_t conn_hdl, BLECharacteristic *chr,
                             uint8_t *data, uint16_t len)
{
  (void) chr;
  if (_midi_instance) _midi_instance->_rxHandler(conn_hdl, data, len);
}

/*------------------------------------------------------------------*/

BLEMidi::BLEMidi(uint16_t fifo_depth)
  : BLEService(BLEMIDI_UUID_SERVICE), _io(BLEMIDI_UUID_CHR_IO)
{
  _write_cb    = NULL;
  _midilib_obj = NULL;

  _rxbuf       = NULL;
  _rxsize      = fifo_depth ? fifo_depth : BLE_MIDI_DEFAULT_FIFO_DEPTH;
  _rxhead      = 0;
  _rxtail      = 0;
  _rx_dropped  = 0;

  memset(_txbuf, 0, sizeof(_txbuf));
  _txcount     = 0;
}

err_t BLEMidi::begin(void)
{
  _midi_instance = this;

  /*
   * FIFO 를 여기서 잡는다. 생성자에서 잡지 않는 이유는 전역 객체의 생성자가
   * 힙이 준비되기 전에 돌 수 있기 때문이다 — 스케치가 BLEMidi 를 전역으로
   * 두는 것이 상류 예제의 기본 형태다.
   */
  if (_rxbuf == NULL) {
    _rxbuf = (uint8_t *) malloc(_rxsize);
    if (_rxbuf == NULL) return NRF_ERROR_NO_MEM;
  }
  _rxhead = _rxtail = 0;

  err_t err = BLEService::begin();
  if (err) return err;

  /*
   * 하나의 characteristic 이 읽기·쓰기·알림을 다 한다 (규격이 그렇다).
   * 보드 -> 호스트는 notify, 호스트 -> 보드는 write 다.
   */
  _io.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP |
                    CHR_PROPS_NOTIFY);
  _io.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  _io.setMaxLen(sdAttMtu() - 3);
  _io.setWriteCallback(blemidi_write_cb);

  err = _io.begin();
  if (err) return err;

  /* 연주 지연을 줄이려고 11.25~15 ms 를 요청한다. 결정은 호스트가 한다. */
  Bluefruit.Periph.setConnInterval(9, 12);
  return NRF_SUCCESS;
}

void BLEMidi::begin(int baudrate)
{
  (void) baudrate;      /* MIDI 라이브러리가 부른다. BLE 에는 보레이트가 없다. */
  this->begin();
}

bool BLEMidi::notifyEnabled(void)
{
  return _io.notifyEnabled(Bluefruit.connHandle());
}

bool BLEMidi::notifyEnabled(uint16_t conn_hdl)
{
  return _io.notifyEnabled(conn_hdl);
}

void BLEMidi::setWriteCallback(midi_write_cb_t fp)
{
  _write_cb = fp;
}

void BLEMidi::autoMIDIread(void *midi_obj)
{
  _midilib_obj = midi_obj;
}

/*------------------------------------------------------------------*/
/* 수신                                                              */
/*------------------------------------------------------------------*/

bool BLEMidi::rxPush(uint8_t b)
{
  uint16_t next = (uint16_t) ((_rxhead + 1) % _rxsize);
  if (next == _rxtail) return false;      /* 가득 참 */
  _rxbuf[_rxhead] = b;
  _rxhead = next;
  return true;
}

void BLEMidi::_rxHandler(uint16_t conn_hdl, uint8_t *data, uint16_t len)
{
  /* 헤더 + 타임스탬프 + 최소 한 바이트는 있어야 의미가 있다. */
  if (len < 3) return;

  data++;                     /* BLE-MIDI 헤더를 버린다 */
  len--;

  while (len) {
    /*
     * bit7 이 선 바이트는 타임스탬프다. MIDI 상태 바이트도 bit7 이 서 있지만,
     * 규격상 여기서 처음 만나는 것은 항상 타임스탬프다.
     *
     * ⚠ 상류는 이 자리에서 `data[1]` 을 조건에 넣는다. 그런데 두 분기가 하는
     *   일이 같고, len == 1 이면 **버퍼 밖을 읽는다.** 결과가 같으므로 조건을
     *   하나로 줄이고 길이를 먼저 본다.
     */
    if (isStatusByte(data[0])) {
      data++;
      len--;
      if (len == 0) break;    /* 타임스탬프만 남은 잘린 패킷 */
    }

    if (!rxPush(data[0])) {
      /* 가득 차면 새 데이터를 버린다 — BLEUart 과 같은 판단이다. */
      _rx_dropped += len;
      break;
    }
    data++;
    len--;
  }

  if (_write_cb) _write_cb(conn_hdl);

#ifdef MIDI_LIB_INCLUDED
  /* 스케치가 autoMIDIread() 를 걸어 뒀으면 받은 자리에서 다 훑는다. */
  if (_midilib_obj) {
    while (((midi::MidiInterface<BLEMidi> *) _midilib_obj)->read()) { }
  }
#endif
}

/*------------------------------------------------------------------*/
/* Stream                                                            */
/*------------------------------------------------------------------*/

int BLEMidi::read(void)
{
  if (_rxbuf == NULL || _rxhead == _rxtail) return -1;
  uint8_t b = _rxbuf[_rxtail];
  _rxtail = (uint16_t) ((_rxtail + 1) % _rxsize);
  return (int) b;
}

int BLEMidi::peek(void)
{
  if (_rxbuf == NULL || _rxhead == _rxtail) return -1;
  return (int) _rxbuf[_rxtail];
}

int BLEMidi::available(void)
{
  if (_rxbuf == NULL) return 0;
  return (int) ((_rxhead + _rxsize - _rxtail) % _rxsize);
}

void BLEMidi::flush(void)
{
  _rxtail = _rxhead;
}

/*
 * MIDI 라이브러리는 메시지를 **한 바이트씩** write() 한다. 그래서 여기서
 * 메시지 하나가 완성될 때까지 모았다가 한 번에 보낸다.
 *
 * ⚠ 상류는 이 버퍼를 함수 안의 `static` 으로 둔다. 인스턴스가 여럿이면 서로
 *   섞이므로 멤버로 옮겼다. 인스턴스가 하나뿐인 보통의 경우 동작은 같다.
 */
size_t BLEMidi::write(uint8_t b)
{
  /*
   * sysex 끝(0xF7)인데 앞에 모아 둔 것이 있으면, 그것부터 비운다.
   * 그래야 0xF7 이 헤더·타임스탬프를 제대로 달고 나간다.
   */
  if (b == 0xF7 && _txcount > 0) {
    if (isStatusByte(_txbuf[0])) send(_txbuf, _txcount);
    else                         sendSplit(_txbuf, _txcount);
    _txbuf[0] = 0;
    _txcount  = 0;
  }

  _txbuf[_txcount++] = b;

  /* 길이가 정해진 메시지는 다 차는 즉시 보낸다. */
  if ((oneByteMessage(_txbuf[0])   && _txcount == 1) ||
      (twoByteMessage(_txbuf[0])   && _txcount == 2) ||
      (threeByteMessage(_txbuf[0]) && _txcount == 3)) {
    send(_txbuf, _txcount);
    _txbuf[0] = 0;
    _txcount  = 0;
  }

  /* 그 밖(sysex 등)은 버퍼가 차면 조각으로 내보낸다. */
  if (_txcount == BLE_MIDI_TX_BUFFER_SIZE) {
    if (isStatusByte(_txbuf[0])) send(_txbuf, _txcount);
    else                         sendSplit(_txbuf, _txcount);
    _txbuf[0] = 0;
    _txcount  = 0;
  }

  return 1;
}

bool BLEMidi::beginTransmission(uint8_t type)
{
  (void) type;
  return true;
}

void BLEMidi::endTransmission(void)
{
}

/*------------------------------------------------------------------*/
/* 메시지 종류                                                       */
/*------------------------------------------------------------------*/

bool BLEMidi::isStatusByte(uint8_t b)
{
  return (b & 0x80) != 0;
}

bool BLEMidi::oneByteMessage(uint8_t status)
{
  if (status >= 0xF4) return true;    /* 시스템 실시간 */
  if (status == 0xF1) return true;    /* MIDI 타임코드 쿼터 프레임 */
  if (status == 0xF7) return true;    /* sysex 끝 */
  return false;
}

bool BLEMidi::twoByteMessage(uint8_t status)
{
  if (status >= 0xC0 && status <= 0xDF) return true;   /* 프로그램 체인지 / 애프터터치 */
  if (status == 0xF3) return true;                     /* 송 셀렉트 */
  return false;
}

bool BLEMidi::threeByteMessage(uint8_t status)
{
  if (status >= 0x80 && status <= 0xBF) return true;   /* 노트 온·오프, 컨트롤 체인지 */
  if (status >= 0xE0 && status <= 0xEF) return true;   /* 피치 벤드 */
  if (status == 0xF2) return true;                     /* 송 포지션 */
  return false;
}

/*------------------------------------------------------------------*/
/* 송신                                                              */
/*------------------------------------------------------------------*/

bool BLEMidi::send(uint8_t data[], uint8_t len)
{
  if (len > BLE_MIDI_TX_BUFFER_SIZE) return false;

  uint32_t tstamp = millis();
  midi_event_packet_t event;
  memset(&event, 0, sizeof(event));

  event.header.timestamp_hi     = (uint8_t) ((tstamp & 0x1F80UL) >> 7);
  event.header.start_bit        = 1;
  event.timestamp.timestamp_low = (uint8_t) (tstamp & 0x7FUL);
  event.timestamp.start_bit     = 1;

  memcpy(event.data, data, len);

  /* 헤더 1 + 타임스탬프 1 + 데이터 */
  return _io.notify(&event, (uint16_t) (len + 2));
}

bool BLEMidi::sendSplit(uint8_t data[], uint8_t len)
{
  if (len > BLE_MIDI_TX_BUFFER_SIZE) return false;

  uint32_t tstamp = millis();
  midi_split_packet_t event;
  memset(&event, 0, sizeof(event));

  event.header.timestamp_hi = (uint8_t) ((tstamp & 0x1F80UL) >> 7);
  event.header.start_bit    = 1;

  memcpy(event.data, data, len);

  /* 헤더 1 + 데이터. 타임스탬프 없음 */
  return _io.notify(&event, (uint16_t) (len + 1));
}
