/*
 * BLEMidi — MIDI over BLE (BLE-MIDI 1.0)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 상류를 따른다 (R12). 스케치는 보통 MIDI 라이브러리와 함께 쓴다:
 *
 *   #include <MIDI.h>
 *   BLEMidi blemidi;
 *   MIDI_CREATE_BLE_INSTANCE(blemidi);     // MIDI 객체를 만든다
 *
 * `Stream` 을 상속해서 MIDI 라이브러리가 이것을 시리얼 포트처럼 쓴다.
 * 라이브러리 없이 `send()` 로 원시 MIDI 바이트를 직접 보내도 된다.
 */
#ifndef _BLE_MIDI_H_
#define _BLE_MIDI_H_

#include <Arduino.h>
#include "BLEService.h"
#include "BLECharacteristic.h"

/** MIDI 라이브러리 인스턴스를 이 서비스 위에 만든다 (상류와 같은 이름). */
#define MIDI_CREATE_BLE_INSTANCE(midiService) \
        MIDI_CREATE_INSTANCE(BLEMidi, midiService, MIDI)

/**
 * 수신 FIFO 크기.
 *
 * ⚠ BLEUart 와 같은 이유로 코어의 RingBuffer 를 쓰지 않는다 — MTU 를 키우면
 *   한 번에 들어오는 양이 64 바이트를 넘는다.
 */
#ifndef BLE_MIDI_DEFAULT_FIFO_DEPTH
#define BLE_MIDI_DEFAULT_FIFO_DEPTH   (128)
#endif

/**
 * 한 패킷에 담는 MIDI 데이터 바이트 수.
 *
 * MIDI 메시지는 최대 3바이트다. 여기에 BLE-MIDI 의 헤더 1 + 타임스탬프 1 이
 * 붙어 5바이트가 나간다.
 */
#define BLE_MIDI_TX_BUFFER_SIZE       (3)

extern const uint8_t BLEMIDI_UUID_SERVICE[];
extern const uint8_t BLEMIDI_UUID_CHR_IO[];

class BLEMidi : public BLEService, public Stream
{
  public:
    typedef void (*midi_write_cb_t)(uint16_t conn_hdl);

    BLEMidi(uint16_t fifo_depth = BLE_MIDI_DEFAULT_FIFO_DEPTH);

    virtual err_t begin(void);

    /**
     * MIDI 라이브러리가 부르는 판. 보레이트는 의미가 없어 무시한다.
     * ⚠ 상류와 같다 — `MIDI.begin()` 이 이것을 부르므로 있어야 한다.
     */
    void begin(int baudrate);

    bool notifyEnabled(void);
    bool notifyEnabled(uint16_t conn_hdl);

    /** 헤더 + 타임스탬프 2바이트를 붙여 보낸다 (상태 바이트로 시작하는 메시지). */
    bool send(uint8_t data[], uint8_t len);

    /** 헤더만 붙여 보낸다 (running status / sysex 이어지는 조각). */
    bool sendSplit(uint8_t data[], uint8_t len);

    /* 메시지 종류 판정 — 상류와 같은 공개 API 다. */
    bool isStatusByte(uint8_t b);
    bool oneByteMessage(uint8_t status);
    bool twoByteMessage(uint8_t status);
    bool threeByteMessage(uint8_t status);

    void setWriteCallback(midi_write_cb_t fp);

    /**
     * 받은 즉시 MIDI 라이브러리의 `read()` 를 돌려 준다.
     * 스케치가 `loop()` 에서 `MIDI.read()` 를 부르지 않아도 콜백이 동작한다.
     */
    void autoMIDIread(void *midi_obj);

    /* MidiInterface 가 요구하는 것. 우리는 패킷을 write() 에서 모은다. */
    bool beginTransmission(uint8_t type);
    void endTransmission(void);

    /* Stream */
    virtual int    read(void);
    virtual size_t write(uint8_t b);
    virtual int    available(void);
    virtual int    peek(void);
    virtual void   flush(void);

    using Print::write;

    /** FIFO 가 넘쳐 버린 바이트 수. 0 이 아니면 FIFO 를 키워라. */
    uint32_t dropped(void) const { return _rx_dropped; }

    /* 내부용 — 정적 콜백이 부른다. */
    void _rxHandler(uint16_t conn_hdl, uint8_t *data, uint16_t len);

  protected:
    BLECharacteristic _io;

    midi_write_cb_t   _write_cb;
    void             *_midilib_obj;

    /* 수신 링버퍼. BLEUart 와 같은 구조다. */
    uint8_t  *_rxbuf;
    uint16_t  _rxsize;
    uint16_t  _rxhead;
    uint16_t  _rxtail;
    uint32_t  _rx_dropped;

    bool rxPush(uint8_t b);

    /* 송신 조립 버퍼 — MIDI 라이브러리가 한 바이트씩 write() 한다. */
    uint8_t _txbuf[BLE_MIDI_TX_BUFFER_SIZE];
    uint8_t _txcount;
};

#endif
