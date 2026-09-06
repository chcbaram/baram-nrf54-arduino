/*
 * BLEBeacon / EddyStoneUrl — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "BLEBeacon.h"
#include "bluefruit.h"
#include "common_func.h"   /* __swap16 */
#include <string.h>

void BLEBeacon::_init(void)
{
  _manufacturer_id = UUID16_COMPANY_ID_APPLE;
  _uuid128    = NULL;
  _major_be   = 0;
  _minor_be   = 0;
  _rssi_at_1m = -54;      /* 규격 예시값 */
}

BLEBeacon::BLEBeacon(void)                       { _init(); }
BLEBeacon::BLEBeacon(uint8_t const uuid128[16])  { _init(); _uuid128 = uuid128; }

BLEBeacon::BLEBeacon(uint8_t const uuid128[16], uint16_t major, uint16_t minor, int8_t rssi)
{
  _init();
  _uuid128    = uuid128;
  _major_be   = __swap16(major);
  _minor_be   = __swap16(minor);
  _rssi_at_1m = rssi;
}

void BLEBeacon::setManufacturer(uint16_t manufacturer) { _manufacturer_id = manufacturer; }
void BLEBeacon::setUuid(uint8_t const uuid128[16])     { _uuid128 = uuid128; }
void BLEBeacon::setRssiAt1m(int8_t rssi)               { _rssi_at_1m = rssi; }

void BLEBeacon::setMajorMinor(uint16_t major, uint16_t minor)
{
  _major_be = __swap16(major);
  _minor_be = __swap16(minor);
}

bool BLEBeacon::start(void)
{
  return start(Bluefruit.Advertising);
}

bool BLEBeacon::start(BLEAdvertising &adv)
{
  if (_uuid128 == NULL) return false;

  /*
   * iBeacon 페이로드는 25바이트로 **고정**이다. 컴파일러가 패딩을 넣으면
   * 스캐너가 못 읽으므로 packed 로 두고 크기를 정적으로 확인한다.
   */
  struct __attribute__((packed)) {
    uint16_t manufacturer;
    uint8_t  beacon_type;
    uint8_t  beacon_len;
    uint8_t  uuid128[16];
    uint16_t major;
    uint16_t minor;
    int8_t   rssi_at_1m;
  } data;
  static_assert(sizeof(data) == 25, "iBeacon payload must be 25 bytes");

  data.manufacturer = _manufacturer_id;
  data.beacon_type  = 0x02;
  data.beacon_len   = (uint8_t) (sizeof(data) - 4);   /* uuid + major + minor + rssi */
  memcpy(data.uuid128, _uuid128, 16);
  data.major        = _major_be;
  data.minor        = _minor_be;
  data.rssi_at_1m   = _rssi_at_1m;

  /* 앞서 쌓아 둔 것이 있으면 자리가 모자라므로 비우고 시작한다. */
  adv.clearData();
  adv.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  return adv.addData(BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, &data, sizeof(data));
}

/* ── EddyStone URL ─────────────────────────────────────────────────── */

/* 규격이 정한 순서다. 인덱스가 곧 인코딩 값이라 바꾸면 안 된다. */
static const char *const prefix_scheme[] = {
  "http://www.", "https://www.", "http://", "https://"
};
enum { PREFIX_COUNT = sizeof(prefix_scheme) / sizeof(prefix_scheme[0]) };

static const char *const url_expansion[] = {
  ".com/", ".org/", ".edu/", ".net/", ".info/", ".biz/", ".gov/",
  ".com",  ".org",  ".edu",  ".net",  ".info",  ".biz",  ".gov",
};
enum { EXPANSION_COUNT = sizeof(url_expansion) / sizeof(url_expansion[0]) };

EddyStoneUrl::EddyStoneUrl(void)                          { _rssi = 0; _url = NULL; }
EddyStoneUrl::EddyStoneUrl(int8_t rssi_at_0m, const char *url) { _rssi = rssi_at_0m; _url = url; }

void EddyStoneUrl::setUrl(const char *url)   { _url = url; }
void EddyStoneUrl::setRssi(int8_t rssi_at_0m) { _rssi = rssi_at_0m; }

/* url 안에서 가장 먼저 나오는 축약 대상. 없으면 NULL. */
static const char *find_expansion(const char *url, uint8_t *idx)
{
  for (uint8_t i = 0; i < EXPANSION_COUNT; i++) {
    const char *hit = strstr(url, url_expansion[i]);
    if (hit) {
      *idx = i;
      return hit;
    }
  }
  return NULL;
}

bool EddyStoneUrl::start(void)
{
  enum { URL_MAXLEN = 17 };

  struct __attribute__((packed)) {
    uint16_t eddy_uuid;
    uint8_t  frame_type;
    int8_t   rssi;
    uint8_t  url_scheme;
    uint8_t  urlencode[URL_MAXLEN];
  } eddy;

  if (_url == NULL) return false;

  eddy.eddy_uuid  = UUID16_SVC_EDDYSTONE;
  eddy.frame_type = EDDYSTONE_TYPE_URL;
  eddy.rssi       = _rssi;
  eddy.url_scheme = 0xFF;
  memset(eddy.urlencode, 0, sizeof(eddy.urlencode));

  const char *url = _url;

  /* 접두사를 코드로 바꾼다. 규격에 없는 스킴이면 실을 수 없다. */
  for (uint8_t i = 0; i < PREFIX_COUNT; i++) {
    size_t prelen = strlen(prefix_scheme[i]);
    if (memcmp(url, prefix_scheme[i], prelen) == 0) {
      eddy.url_scheme = i;
      url += prelen;
      break;
    }
  }
  if (eddy.url_scheme >= PREFIX_COUNT) return false;

  uint8_t len = 0;

  while (*url) {
    uint8_t ex_code = 0;
    const char *expansion = find_expansion(url, &ex_code);

    /* 축약 대상 앞까지 그대로 복사한다. 축약이 있으면 코드 1바이트도 남겨 둔다. */
    size_t cp_num = expansion ? (size_t) (expansion - url) : strlen(url);
    if (cp_num > (size_t) (URL_MAXLEN - (len + (expansion ? 1 : 0)))) {
      return false;         /* 압축해도 17바이트를 넘는다 */
    }

    memcpy(eddy.urlencode + len, url, cp_num);
    url += cp_num;
    len += (uint8_t) cp_num;

    if (expansion) {
      eddy.urlencode[len++] = ex_code;
      url += strlen(url_expansion[ex_code]);
    }
  }

  Bluefruit.Advertising.clearData();
  if (!Bluefruit.Advertising.addUuid(BLEUuid(UUID16_SVC_EDDYSTONE))) return false;

  /* uuid(2) + frame_type(1) + rssi(1) + scheme(1) = 5 바이트가 URL 앞에 붙는다. */
  return Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, &eddy, (uint8_t) (len + 5));
}
