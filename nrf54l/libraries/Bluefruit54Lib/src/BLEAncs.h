/*
 * BLEAncs — Apple Notification Center Service
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * iPhone 의 알림을 보드로 가져온다. 보드가 **peripheral 인데 GATT 클라이언트**인
 * 구성이라 BLEClientCts 와 같은 모양이다 (그래서 예제도 Peripheral 아래 둔다).
 *
 * ⚠ iOS 는 ANCS 를 **본딩된 상대에게만** 열어 준다. 그리고 광고에 이 서비스를
 *   solicitation 으로 실어야 한다 — `Advertising.addService(bleancs)` 가 그 일을 한다.
 *
 * API 는 상류를 따른다 (R12).
 */
#ifndef _BLE_ANCS_H_
#define _BLE_ANCS_H_

#include "BLEClientService.h"
#include "BLEClientCharacteristic.h"

extern const uint8_t BLEANCS_UUID_SERVICE[];
extern const uint8_t BLEANCS_UUID_CHR_CONTROL[];
extern const uint8_t BLEANCS_UUID_CHR_NOTIFICATION[];
extern const uint8_t BLEANCS_UUID_CHR_DATA[];

/* 알림 분류 */
enum {
  ANCS_CAT_OTHER = 0,
  ANCS_CAT_INCOMING_CALL,
  ANCS_CAT_MISSED_CALL,
  ANCS_CAT_VOICE_MAIL,
  ANCS_CAT_SOCIAL,
  ANCS_CAT_SCHEDULE,
  ANCS_CAT_EMAIL,
  ANCS_CAT_NEWS,
  ANCS_CAT_HEALTH_AND_FITNESS,
  ANCS_CAT_BUSSINESS_AND_FINANCE,   /* 상류 철자 그대로 둔다 (R12) */
  ANCS_CAT_LOCATION,
  ANCS_CAT_ENTERTAINMENT
};

/* 알림에 생긴 일 */
enum {
  ANCS_EVT_NOTIFICATION_ADDED = 0,
  ANCS_EVT_NOTIFICATION_MODIFIED,
  ANCS_EVT_NOTIFICATION_REMOVED
};

enum {
  ANCS_CMD_GET_NOTIFICATION_ATTR = 0,
  ANCS_CMD_GET_APP_ATTR,
  ANCS_CMD_PERFORM_NOTIFICATION_ACTION
};

/* 알림 속성. TITLE / SUBTITLE / MESSAGE 는 요청에 2바이트 길이가 더 붙는다. */
enum {
  ANCS_ATTR_APP_IDENTIFIER = 0,
  ANCS_ATTR_TITLE,
  ANCS_ATTR_SUBTITLE,
  ANCS_ATTR_MESSAGE,
  ANCS_ATTR_MESSAGE_SIZE,
  ANCS_ATTR_DATE,                   /* UTC#35 yyyyMMdd'T'HHmmSS */
  ANCS_ATTR_POSITIVE_ACTION_LABEL,
  ANCS_ATTR_NEGATIVE_ACTION_LABEL,

  ANCS_ATTR_INVALID
};

enum { ANCS_ACTION_POSITIVE = 0, ANCS_ACTION_NEGATIVE };

enum { ANCS_APP_ATTR_DISPLAY_NAME = 0, ANCS_APP_ATTR_INVALID };

/** Notification Source 가 보내는 8바이트. */
typedef struct {
  uint8_t eventID;

  struct __attribute__((packed)) {
    uint8_t silent         : 1;
    uint8_t important      : 1;
    uint8_t preExisting    : 1;
    uint8_t positiveAction : 1;
    uint8_t NegativeAction : 1;    /* 상류 철자 그대로 (R12) */
  } eventFlags;

  uint8_t  categoryID;
  uint8_t  categoryCount;
  uint32_t uid;
} AncsNotification_t;

static_assert(sizeof(AncsNotification_t) == 8, "ANCS notification is 8 bytes");

class BLEAncs : public BLEClientService
{
  public:
    typedef void (*notification_callback_t)(AncsNotification_t *notif);

    BLEAncs(void);

    virtual bool begin(void);
    virtual bool discover(uint16_t conn_hdl);

    /**
     * 알림이 올 때 부를 함수.
     *
     * ⚠ 이 콜백은 **이벤트 태스크가 아닌 콜백 태스크**에서 돈다. 그래야 이 안에서
     *   `getTitle()` 같은 블로킹 호출을 할 수 있다 — 그게 ANCS 를 쓰는 보통의 방식이다.
     */
    void setNotificationCallback(notification_callback_t fp);

    bool enableNotification(void);
    bool disableNotification(void);

    /* 원시 명령 */
    uint16_t getAttribute(uint32_t uid, uint8_t attr, void *buffer, uint16_t bufsize);
    uint16_t getAppAttribute(const char *appid, uint8_t attr, void *buffer, uint16_t bufsize);
    bool     performAction(uint32_t uid, uint8_t actionid);

    /* 편의 */
    uint16_t getAppName(uint32_t uid, void *buffer, uint16_t bufsize);
    uint16_t getAppID(uint32_t uid, void *buffer, uint16_t bufsize);
    uint16_t getTitle(uint32_t uid, void *buffer, uint16_t bufsize);
    uint16_t getSubtitle(uint32_t uid, void *buffer, uint16_t bufsize);
    uint16_t getMessage(uint32_t uid, void *buffer, uint16_t bufsize);
    uint16_t getMessageSize(uint32_t uid);
    uint16_t getDate(uint32_t uid, void *buffer, uint16_t bufsize);
    uint16_t getPosActionLabel(uint32_t uid, void *buffer, uint16_t bufsize);
    uint16_t getNegActionLabel(uint32_t uid, void *buffer, uint16_t bufsize);

    bool actPositive(uint32_t uid);
    bool actNegative(uint32_t uid);

    /* 내부용 — 정적 콜백이 부른다. */
    void _handleNotification(const uint8_t *data, uint16_t len);
    void _handleData(uint8_t *data, uint16_t len);

  protected:
    BLEClientCharacteristic _control;
    BLEClientCharacteristic _notification;
    BLEClientCharacteristic _data;

    notification_callback_t _notif_cb;

    /*
     * 조각으로 오는 응답을 모으는 자리. 상류의 AdaMsg 를 대신한다 —
     * 그 유틸리티가 우리 저장소에 없어 필요한 만큼만 직접 뒀다.
     */
    uint8_t  *_rx_buf;
    uint16_t  _rx_size;      /* 버퍼 크기 */
    uint16_t  _rx_len;       /* 지금까지 담긴 양 */
    void     *_rx_sem;       /* SemaphoreHandle_t — 헤더에 FreeRTOS 를 안 들인다 */

    void     _rxPrepare(void *buffer, uint16_t bufsize);
    bool     _rxWait(uint32_t ms);
    uint16_t _rxRemaining(void) const { return (uint16_t) (_rx_size - _rx_len); }
};

#endif
