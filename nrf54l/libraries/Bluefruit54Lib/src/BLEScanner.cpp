/*
 * BLEScanner — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

BLEScanner::BLEScanner(void)
{
  memset(&_param, 0, sizeof(_param));
  _param.active        = 1;                       /* 스캔 응답까지 받는다 */
  _param.interval      = BLE_SCAN_INTERVAL_DFLT;
  _param.window        = BLE_SCAN_WINDOW_DFLT;
  _param.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
  _param.scan_phys     = BLE_GAP_PHY_1MBPS;
  _param.timeout       = 0;

  _report_data.p_data = _scan_data;
  _report_data.len    = BLE_GAP_SCAN_BUFFER_MAX;

  _running = false;
  _restart = true;

  _filter_rssi       = -128;      /* 사실상 없음 */
  _filter_msd_en     = false;
  _filter_msd_id     = 0;
  _filter_uuid_count = 0;

  _rx_cb   = NULL;
  _stop_cb = NULL;
}

void BLEScanner::setInterval(uint16_t interval, uint16_t window)
{
  _param.interval = interval;
  _param.window   = window;
}

void BLEScanner::setIntervalMS(uint16_t interval_ms, uint16_t window_ms)
{
  /* 0.625 ms 단위. 내림하면 규격 최소치를 밑돌 수 있어 올림한다. */
  _param.interval = (uint16_t) ((interval_ms * 1600 + 999) / 1000);
  _param.window   = (uint16_t) ((window_ms   * 1600 + 999) / 1000);
}

void BLEScanner::filterMSD(uint16_t manuf_id)
{
  _filter_msd_en = true;
  _filter_msd_id = manuf_id;
}

void BLEScanner::filterUuid(BLEUuid uuid)
{
  BLEUuid one[1] = { uuid };
  filterUuid(one, 1);
}

void BLEScanner::filterUuid(BLEUuid uuid1, BLEUuid uuid2)
{
  BLEUuid two[2] = { uuid1, uuid2 };
  filterUuid(two, 2);
}

void BLEScanner::filterUuid(BLEUuid uuid[], uint8_t count)
{
  if (count > BLE_SCAN_FILTER_UUID_MAX) count = BLE_SCAN_FILTER_UUID_MAX;

  for (uint8_t i = 0; i < count; i++) {
    _filter_uuid[i] = uuid[i];
    /* 128비트 UUID 는 SoftDevice 에 등록돼야 비교가 된다. */
    _filter_uuid[i].begin();
  }
  _filter_uuid_count = count;
}

void BLEScanner::filterService(BLEService &svc)
{
  filterUuid(svc.uuid);
}

void BLEScanner::filterService(BLEClientService &svc)
{
  filterUuid(svc.uuid);
}

void BLEScanner::clearFilters(void)
{
  _filter_rssi       = -128;
  _filter_msd_en     = false;
  _filter_uuid_count = 0;
}

bool BLEScanner::start(uint16_t timeout)
{
  /*
   * ⚠ 보고 버퍼는 SoftDevice 가 **포인터로 들고 있는다.** 스택에 두면
   *   스캔이 도는 동안 엉뚱한 메모리에 쓴다. 그래서 멤버다.
   */
  _report_data.p_data = _scan_data;
  _report_data.len    = BLE_GAP_SCAN_BUFFER_MAX;
  _param.timeout      = timeout;

  uint32_t err = sd_ble_gap_scan_start(&_param, &_report_data);
  _running = (err == NRF_SUCCESS);
  return _running;
}

bool BLEScanner::resume(void)
{
  /* 파라미터에 NULL 을 주면 "멈춘 스캔을 그대로 이어 간다" 는 뜻이다. */
  _report_data.p_data = _scan_data;
  _report_data.len    = BLE_GAP_SCAN_BUFFER_MAX;
  return sd_ble_gap_scan_start(NULL, &_report_data) == NRF_SUCCESS;
}

bool BLEScanner::stop(void)
{
  _running = false;
  return sd_ble_gap_scan_stop() == NRF_SUCCESS;
}

/* ── 광고 데이터 파서 ─────────────────────────────────────────────── */

uint8_t BLEScanner::parseReportByType(const uint8_t *data, uint8_t len, uint8_t type,
                                      uint8_t *buf, uint8_t bufsize)
{
  if (data == NULL) return 0;

  uint8_t i = 0;

  /* AD 구조는 [길이][타입][값...] 이 이어진 것이다. 길이는 타입을 포함한다. */
  while (i < len) {
    uint8_t field_len = data[i];

    /* 길이 0 은 데이터 끝을 뜻한다. 넘어가면 남의 메모리를 읽는다. */
    if (field_len == 0)        break;
    if (i + field_len >= len + 1U) break;

    uint8_t field_type = data[i + 1];
    uint8_t value_len  = (uint8_t) (field_len - 1);

    if (field_type == type) {
      if (buf && bufsize) {
        uint8_t n = (value_len > bufsize) ? bufsize : value_len;
        memcpy(buf, &data[i + 2], n);
      }
      return value_len;
    }
    i += field_len + 1;
  }
  return 0;
}

uint8_t BLEScanner::parseReportByType(const ble_gap_evt_adv_report_t *report, uint8_t type,
                                      uint8_t *buf, uint8_t bufsize)
{
  if (report == NULL) return 0;
  return parseReportByType(report->data.p_data, report->data.len, type, buf, bufsize);
}

bool BLEScanner::checkReportForUuid(const ble_gap_evt_adv_report_t *report, BLEUuid uuid)
{
  if (report == NULL) return false;

  const uint8_t *data = report->data.p_data;
  const uint8_t  len  = report->data.len;

  if (uuid.size() == 16) {
    /* 16비트 UUID 목록. 완전/부분 둘 다 본다. */
    const uint8_t types[2] = {
      BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE,
      BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE
    };
    uint8_t buf[BLE_GAP_SCAN_BUFFER_MAX];

    for (uint8_t t = 0; t < 2; t++) {
      uint8_t n = parseReportByType(data, len, types[t], buf, sizeof(buf));
      for (uint8_t i = 0; i + 1 < n; i += 2) {
        uint16_t u16 = (uint16_t) (buf[i] | (buf[i + 1] << 8));
        if (u16 == uuid._uuid.uuid) return true;
      }
    }
    return false;
  }

  if (uuid._uuid128 == NULL) return false;

  const uint8_t types[2] = {
    BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE,
    BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE
  };
  uint8_t buf[BLE_GAP_SCAN_BUFFER_MAX];

  for (uint8_t t = 0; t < 2; t++) {
    uint8_t n = parseReportByType(data, len, types[t], buf, sizeof(buf));
    for (uint8_t i = 0; i + 16 <= n; i += 16) {
      if (memcmp(&buf[i], uuid._uuid128, 16) == 0) return true;
    }
  }
  return false;
}

bool BLEScanner::checkReportForService(const ble_gap_evt_adv_report_t *report, BLEService &svc)
{
  return checkReportForUuid(report, svc.uuid);
}

bool BLEScanner::checkReportForService(const ble_gap_evt_adv_report_t *report, BLEClientService &svc)
{
  return checkReportForUuid(report, svc.uuid);
}

bool BLEScanner::passesFilters(const ble_gap_evt_adv_report_t *report)
{
  if (report->rssi < _filter_rssi) return false;

  if (_filter_msd_en) {
    uint8_t buf[BLE_GAP_SCAN_BUFFER_MAX];
    uint8_t n = parseReportByType(report, BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA,
                                  buf, sizeof(buf));
    if (n < 2) return false;
    if ((uint16_t) (buf[0] | (buf[1] << 8)) != _filter_msd_id) return false;
  }

  if (_filter_uuid_count > 0) {
    bool hit = false;
    for (uint8_t i = 0; i < _filter_uuid_count && !hit; i++) {
      hit = checkReportForUuid(report, _filter_uuid[i]);
    }
    if (!hit) return false;
  }
  return true;
}

void BLEScanner::_eventHandler(const ble_evt_t *evt)
{
  switch (evt->header.evt_id) {
    case BLE_GAP_EVT_ADV_REPORT: {
      const ble_gap_evt_adv_report_t *report = &evt->evt.gap_evt.params.adv_report;

      if (!passesFilters(report)) {
        /*
         * 거른 보고도 **스캔을 다시 켜 줘야 한다.** 콜백을 안 부르면
         * 스케치의 resume() 도 안 불리므로 여기서 이어 간다.
         * 빠뜨리면 필터에 처음 걸린 순간 스캔이 멈춰 버린다.
         */
        resume();
        break;
      }

      /*
       * ⚠ 이 콜백은 **BLE 이벤트 태스크에서 직접** 불린다 (연결 콜백과 다르다).
       *   report 가 가리키는 것이 이벤트 버퍼라, 미루면 그 사이에 덮인다.
       *   그래서 콜백 안에서 **블로킹 호출을 하면 안 된다** — getPeerName() 같은
       *   GATT 읽기는 여기서 부르지 마라. 출력·필터·Central.connect() 는 괜찮다.
       */
      if (_rx_cb) _rx_cb((ble_gap_evt_adv_report_t *) report);
      break;
    }

    case BLE_GAP_EVT_TIMEOUT:
      if (evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN) {
        _running = false;
        if (_stop_cb) _stop_cb();
      }
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      if (_restart && !_running) start(_param.timeout);
      break;

    default:
      break;
  }
}
