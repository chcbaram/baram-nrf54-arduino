/*
 * BLEUuid16.h — Bluetooth SIG 표준 16비트 UUID
 * baram-nrf54-arduino / SPDX-License-Identifier: MIT
 *
 * Adafruit Bluefruit52Lib 의 BLEUuid.h 가 정의하는 이름과 값을 따른다.
 * 예제가 이 이름들을 그대로 쓴다.
 *
 * ⚠ 전부가 아니라 **예제에서 실제로 쓰이는 것과 흔한 것만** 넣었다.
 *   Bluetooth SIG 의 Assigned Numbers 에서 필요한 것을 추가하면 된다.
 */
#ifndef _BLE_UUID16_H_
#define _BLE_UUID16_H_

/* ── 서비스 ─────────────────────────────────────────────────────────── */
#define UUID16_SVC_GENERIC_ACCESS                 0x1800
#define UUID16_SVC_GENERIC_ATTRIBUTE              0x1801
#define UUID16_SVC_IMMEDIATE_ALERT                0x1802
#define UUID16_SVC_LINK_LOSS                      0x1803
#define UUID16_SVC_TX_POWER                       0x1804
#define UUID16_SVC_CURRENT_TIME                   0x1805
#define UUID16_SVC_DEVICE_INFORMATION             0x180A
#define UUID16_SVC_HEART_RATE                     0x180D
#define UUID16_SVC_BATTERY_SERVICE                0x180F
#define UUID16_SVC_HEALTH_THERMOMETER             0x1809
#define UUID16_SVC_HUMAN_INTERFACE_DEVICE         0x1812

/* ── characteristic ─────────────────────────────────────────────────── */
#define UUID16_CHR_DEVICE_NAME                    0x2A00
#define UUID16_CHR_APPEARANCE                     0x2A01
#define UUID16_CHR_BATTERY_LEVEL                  0x2A19
#define UUID16_CHR_TEMPERATURE_MEASUREMENT        0x2A1C
#define UUID16_CHR_TEMPERATURE_TYPE               0x2A1D
#define UUID16_CHR_SYSTEM_ID                      0x2A23
#define UUID16_CHR_MODEL_NUMBER_STRING            0x2A24
#define UUID16_CHR_SERIAL_NUMBER_STRING           0x2A25
#define UUID16_CHR_FIRMWARE_REVISION_STRING       0x2A26
#define UUID16_CHR_HARDWARE_REVISION_STRING       0x2A27
#define UUID16_CHR_SOFTWARE_REVISION_STRING       0x2A28
#define UUID16_CHR_MANUFACTURER_NAME_STRING       0x2A29
#define UUID16_CHR_CURRENT_TIME                   0x2A2B
#define UUID16_CHR_HEART_RATE_MEASUREMENT         0x2A37
#define UUID16_CHR_BODY_SENSOR_LOCATION           0x2A38
#define UUID16_CHR_HEART_RATE_CONTROL_POINT       0x2A39
#define UUID16_CHR_HID_INFORMATION                0x2A4A
#define UUID16_CHR_REPORT_MAP                     0x2A4B
#define UUID16_CHR_HID_CONTROL_POINT              0x2A4C
#define UUID16_CHR_REPORT                         0x2A4D
#define UUID16_CHR_PROTOCOL_MODE                  0x2A4E
#define UUID16_CHR_PNP_ID                         0x2A50

/* ── 회사 식별자 (Bluetooth SIG Company Identifiers) ─────────────────── */
#define UUID16_COMPANY_ID_ADAFRUIT                0x0822
#define UUID16_COMPANY_ID_NORDIC                  0x0059

#endif
