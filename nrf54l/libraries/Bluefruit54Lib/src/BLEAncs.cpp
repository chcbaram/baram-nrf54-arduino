/*
 * BLEAncs — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "semphr.h"

/** 명령 하나에 허용하는 시간. 아이폰이 조각으로 나눠 보내므로 넉넉히 준다. */
#define BLE_ANCS_TIMEOUT_MS   (5000)

const uint8_t BLEANCS_UUID_SERVICE[] =
{
  0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
  0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79
};

const uint8_t BLEANCS_UUID_CHR_CONTROL[] =
{
  0xD9, 0xD9, 0xAA, 0xFD, 0xBD, 0x9B, 0x21, 0x98,
  0xA8, 0x49, 0xE1, 0x45, 0xF3, 0xD8, 0xD1, 0x69
};

const uint8_t BLEANCS_UUID_CHR_NOTIFICATION[] =
{
  0xBD, 0x1D, 0xA2, 0x99, 0xE6, 0x25, 0x58, 0x8C,
  0xD9, 0x42, 0x01, 0x63, 0x0D, 0x12, 0xBF, 0x9F
};

const uint8_t BLEANCS_UUID_CHR_DATA[] =
{
  0xFB, 0x7B, 0x7C, 0xCE, 0x6A, 0xB3, 0x44, 0xBE,
  0xB5, 0x4B, 0xD6, 0x24, 0xE9, 0xC6, 0xEA, 0x22
};

/* 응답 앞에 붙어 오는 머리. 요청을 그대로 되돌려 주고 길이가 따라온다. */
typedef struct __attribute__((packed)) {
  uint8_t  cmd;
  uint32_t uid;
  uint8_t  attr;
  uint16_t len;
} get_notif_attr_t;

typedef struct __attribute__((packed)) {
  uint8_t  cmd;
  uint32_t uid;
  uint8_t  actionid;
} perform_action_t;

static_assert(sizeof(get_notif_attr_t) == 8, "command layout");
static_assert(sizeof(perform_action_t) == 6, "command layout");

/*------------------------------------------------------------------*/
/* 콜백                                                              */
/*------------------------------------------------------------------*/

/* 콜백 태스크에서 불린다 (bluefruit.cpp 의 BLE_CB_DEFER). */
static void ancs_notif_deferred(void *ctx, const uint8_t *data, uint16_t len)
{
  ((BLEAncs *) ctx)->_handleNotification(data, len);
}

/*
 * Notification Source. **이벤트 태스크에서 불린다.**
 *
 * ⚠ 여기서 스케치 콜백을 바로 부르면 안 된다 — 스케치는 그 안에서 제목·본문을
 *   가져오는데, 그것이 Data Source 알림을 기다리는 블로킹 절차다. 이벤트 태스크에서
 *   기다리면 그 알림을 처리할 주체가 자기 자신이라 영영 안 온다.
 *   (같은 함정을 §2.8 보안 콜백에서 이미 겪었다.)
 */
static void ancs_notification_cb(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
{
  BLEAncs *ancs = (BLEAncs *) chr->parentService();
  if (ancs) Bluefruit._deferCallback(ancs_notif_deferred, ancs, data, len);
}

/*
 * Data Source. **이벤트 태스크에서 그대로 처리한다** — 위 콜백이 콜백 태스크에서
 * 기다리는 동안 이쪽이 계속 채워 줘야 하기 때문이다. 둘을 같은 태스크에 두면 막힌다.
 */
static void ancs_data_cb(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
{
  BLEAncs *ancs = (BLEAncs *) chr->parentService();
  if (ancs) ancs->_handleData(data, len);
}

/*------------------------------------------------------------------*/

BLEAncs::BLEAncs(void)
  : BLEClientService(BLEUuid(BLEANCS_UUID_SERVICE)),
    _control(BLEUuid(BLEANCS_UUID_CHR_CONTROL)),
    _notification(BLEUuid(BLEANCS_UUID_CHR_NOTIFICATION)),
    _data(BLEUuid(BLEANCS_UUID_CHR_DATA))
{
  _notif_cb = NULL;
  _rx_buf   = NULL;
  _rx_size  = 0;
  _rx_len   = 0;
  _rx_sem   = NULL;
}

bool BLEAncs::begin(void)
{
  if (!BLEClientService::begin()) return false;

  if (_rx_sem == NULL) {
    _rx_sem = (void *) xSemaphoreCreateBinary();
    if (_rx_sem == NULL) return false;
  }

  _control.begin(this);
  _notification.begin(this);
  _data.begin(this);

  _notification.setNotifyCallback(ancs_notification_cb);
  _data.setNotifyCallback(ancs_data_cb);
  return true;
}

bool BLEAncs::discover(uint16_t conn_hdl)
{
  if (!BLEClientService::discover(conn_hdl)) return false;

  discoverCharacteristics();

  /* 셋 다 있어야 쓸 수 있다. 하나라도 없으면 ANCS 가 아니다. */
  if (!_control.discovered() || !_notification.discovered() || !_data.discovered()) {
    _disconnected();
    return false;
  }
  return true;
}

void BLEAncs::setNotificationCallback(notification_callback_t fp)
{
  _notif_cb = fp;
}

bool BLEAncs::enableNotification(void)
{
  /* Data 를 먼저 켠다 — 알림이 먼저 오면 답을 받을 자리가 없다. */
  if (!_data.enableNotify()) return false;
  return _notification.enableNotify();
}

bool BLEAncs::disableNotification(void)
{
  _notification.disableNotify();
  _data.disableNotify();
  return true;
}

/*------------------------------------------------------------------*/
/* 조각 모으기 (상류 AdaMsg 자리)                                     */
/*------------------------------------------------------------------*/

void BLEAncs::_rxPrepare(void *buffer, uint16_t bufsize)
{
  _rx_buf  = (uint8_t *) buffer;
  _rx_size = bufsize;
  _rx_len  = 0;

  /* 앞선 명령이 남긴 신호를 비운다. 안 그러면 첫 wait 가 즉시 통과한다. */
  while (xSemaphoreTake((SemaphoreHandle_t) _rx_sem, 0) == pdTRUE) { }
}

bool BLEAncs::_rxWait(uint32_t ms)
{
  return xSemaphoreTake((SemaphoreHandle_t) _rx_sem, pdMS_TO_TICKS(ms)) == pdTRUE;
}

void BLEAncs::_handleData(uint8_t *data, uint16_t len)
{
  if (_rx_buf && len) {
    uint16_t room = _rxRemaining();
    uint16_t n    = (len > room) ? room : len;
    memcpy(_rx_buf + _rx_len, data, n);
    _rx_len += n;
  }
  /* 조각마다 깨운다 — 부르는 쪽이 길이를 보고 더 기다릴지 정한다. */
  xSemaphoreGive((SemaphoreHandle_t) _rx_sem);
}

void BLEAncs::_handleNotification(const uint8_t *data, uint16_t len)
{
  if (len != sizeof(AncsNotification_t)) return;
  if (_notif_cb) _notif_cb((AncsNotification_t *) data);
}

/*------------------------------------------------------------------*/
/* 명령                                                              */
/*------------------------------------------------------------------*/

uint16_t BLEAncs::getAttribute(uint32_t uid, uint8_t attr, void *buffer, uint16_t bufsize)
{
  if (attr >= ANCS_ATTR_INVALID || buffer == NULL || bufsize == 0) return 0;

  get_notif_attr_t command;
  memset(&command, 0, sizeof(command));
  command.cmd  = ANCS_CMD_GET_NOTIFICATION_ATTR;
  command.uid  = uid;
  command.attr = attr;
  command.len  = bufsize;

  /* 길이를 붙이는 것은 문자열 셋뿐이다. 나머지에 붙이면 아이폰이 거절한다. */
  uint8_t cmdlen = 6;
  if (attr == ANCS_ATTR_TITLE || attr == ANCS_ATTR_SUBTITLE || attr == ANCS_ATTR_MESSAGE) {
    cmdlen = 8;
  }

  _rxPrepare(buffer, bufsize);
  if (_control.write_resp(&command, cmdlen) != cmdlen) return 0;
  if (!_rxWait(BLE_ANCS_TIMEOUT_MS)) return 0;

  /* 첫 조각이면 머리가 다 왔다 — 거기 실제 길이가 있다. */
  if (_rx_len < sizeof(get_notif_attr_t)) return 0;
  uint16_t attr_len = ((get_notif_attr_t *) buffer)->len;

  /* 다 올 때까지, 또는 버퍼가 찰 때까지 기다린다. */
  while (((uint32_t) attr_len + sizeof(get_notif_attr_t)) > _rx_len && _rxRemaining() > 0) {
    if (!_rxWait(BLE_ANCS_TIMEOUT_MS)) return 0;
  }

  /* 버퍼가 모자랐으면 받은 만큼이 답이다. */
  attr_len = (uint16_t) (_rx_len - sizeof(get_notif_attr_t));

  /* 머리를 걷어내고 속성만 남긴다. */
  memmove(buffer, ((uint8_t *) buffer) + sizeof(get_notif_attr_t), attr_len);

  /* 문자열로 쓰는 스케치가 많아 자리가 있으면 끝을 맺어 준다. */
  if (attr_len < bufsize) ((char *) buffer)[attr_len] = 0;

  return attr_len;
}

uint16_t BLEAncs::getAppAttribute(const char *appid, uint8_t attr, void *buffer, uint16_t bufsize)
{
  if (attr >= ANCS_APP_ATTR_INVALID || appid == NULL || buffer == NULL || bufsize == 0) return 0;

  /* 명령 = cmd + appid(널 포함) + attr */
  uint16_t idlen  = (uint16_t) strlen(appid);
  uint16_t cmdlen = (uint16_t) (1 + idlen + 1 + 1);

  uint8_t *command = (uint8_t *) malloc(cmdlen);
  if (command == NULL) return 0;

  command[0] = ANCS_CMD_GET_APP_ATTR;
  memcpy(command + 1, appid, idlen + 1);
  command[cmdlen - 1] = attr;

  _rxPrepare(buffer, bufsize);
  bool sent = (_control.write_resp(command, cmdlen) == cmdlen);
  free(command);
  if (!sent) return 0;

  /*
   * 여기 응답 머리는 요청을 되돌려 준 것이라 **길이가 요청 길이에 달려 있다** —
   * 알림 속성처럼 고정 8바이트가 아니다. 그래서 cmdlen + 2 를 쓴다.
   */
  while ((uint32_t) (cmdlen + 2) > _rx_len && _rxRemaining() > 0) {
    if (!_rxWait(BLE_ANCS_TIMEOUT_MS)) return 0;
  }
  if (_rx_len < (uint32_t) (cmdlen + 2)) return 0;

  uint16_t attr_len;
  memcpy(&attr_len, ((uint8_t *) buffer) + cmdlen, 2);

  while (((uint32_t) attr_len + cmdlen + 2) > _rx_len && _rxRemaining() > 0) {
    if (!_rxWait(BLE_ANCS_TIMEOUT_MS)) return 0;
  }

  attr_len = (uint16_t) (_rx_len - (cmdlen + 2));
  memmove(buffer, ((uint8_t *) buffer) + cmdlen + 2, attr_len);
  if (attr_len < bufsize) ((char *) buffer)[attr_len] = 0;

  return attr_len;
}

bool BLEAncs::performAction(uint32_t uid, uint8_t actionid)
{
  perform_action_t action;
  memset(&action, 0, sizeof(action));
  action.cmd      = ANCS_CMD_PERFORM_NOTIFICATION_ACTION;
  action.uid      = uid;
  action.actionid = actionid;

  return _control.write_resp(&action, sizeof(action)) == sizeof(action);
}

/*------------------------------------------------------------------*/
/* 편의                                                              */
/*------------------------------------------------------------------*/

uint16_t BLEAncs::getAppID(uint32_t uid, void *buffer, uint16_t bufsize)
{ return getAttribute(uid, ANCS_ATTR_APP_IDENTIFIER, buffer, bufsize); }

uint16_t BLEAncs::getTitle(uint32_t uid, void *buffer, uint16_t bufsize)
{ return getAttribute(uid, ANCS_ATTR_TITLE, buffer, bufsize); }

uint16_t BLEAncs::getSubtitle(uint32_t uid, void *buffer, uint16_t bufsize)
{ return getAttribute(uid, ANCS_ATTR_SUBTITLE, buffer, bufsize); }

uint16_t BLEAncs::getMessage(uint32_t uid, void *buffer, uint16_t bufsize)
{ return getAttribute(uid, ANCS_ATTR_MESSAGE, buffer, bufsize); }

uint16_t BLEAncs::getDate(uint32_t uid, void *buffer, uint16_t bufsize)
{ return getAttribute(uid, ANCS_ATTR_DATE, buffer, bufsize); }

uint16_t BLEAncs::getPosActionLabel(uint32_t uid, void *buffer, uint16_t bufsize)
{ return getAttribute(uid, ANCS_ATTR_POSITIVE_ACTION_LABEL, buffer, bufsize); }

uint16_t BLEAncs::getNegActionLabel(uint32_t uid, void *buffer, uint16_t bufsize)
{ return getAttribute(uid, ANCS_ATTR_NEGATIVE_ACTION_LABEL, buffer, bufsize); }

uint16_t BLEAncs::getMessageSize(uint32_t uid)
{
  char buf[20] = { 0 };
  if (!getAttribute(uid, ANCS_ATTR_MESSAGE_SIZE, buf, sizeof(buf))) return 0;
  return (uint16_t) strtoul(buf, NULL, 10);
}

uint16_t BLEAncs::getAppName(uint32_t uid, void *buffer, uint16_t bufsize)
{
  /* 앱 이름은 두 번 물어야 한다 — 먼저 앱 ID, 그걸로 표시 이름. */
  char appid[64] = { 0 };
  if (!getAppID(uid, appid, sizeof(appid))) return 0;
  return getAppAttribute(appid, ANCS_APP_ATTR_DISPLAY_NAME, buffer, bufsize);
}

bool BLEAncs::actPositive(uint32_t uid) { return performAction(uid, ANCS_ACTION_POSITIVE); }
bool BLEAncs::actNegative(uint32_t uid) { return performAction(uid, ANCS_ACTION_NEGATIVE); }
