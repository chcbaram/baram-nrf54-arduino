/*
 * ble_hid_defs.h — HID 리포트 정의
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit 은 이 정의들을 **TinyUSB 에서 빌려 쓴다** (`class/hid/hid.h` 의
 * `hid_keyboard_report_t`, `HID_KEY_*`, `TUD_HID_REPORT_DESC_*` 매크로).
 * nRF52840 은 USB 하드웨어가 있어 TinyUSB 를 어차피 싣기 때문이다.
 *
 * ⚠ 우리는 그럴 수 없다. **nRF54L15 에는 USB 하드웨어가 없고**, TinyUSB 의
 *   hid.h 는 165 KB 에 `common/tusb_common.h` 를 끌고 온다. HID 상수 몇 개
 *   때문에 USB 스택을 통째로 들일 이유가 없어 필요한 것만 여기 적는다.
 *   이름은 상류와 같게 두어 예제가 그대로 컴파일되게 한다 (R12).
 */
#ifndef _BLE_HID_DEFS_H_
#define _BLE_HID_DEFS_H_

#include <stdint.h>

/* ── 리포트 구조 ───────────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
  uint8_t modifier;      /**< HID_KEYBOARD_MODIFIER_* 비트합 */
  uint8_t reserved;
  uint8_t keycode[6];    /**< 동시에 눌린 키. 0 이면 빈 자리 */
} hid_keyboard_report_t;

typedef struct __attribute__((packed)) {
  uint8_t buttons;       /**< MOUSE_BUTTON_* 비트합 */
  int8_t  x;
  int8_t  y;
  int8_t  wheel;         /**< 세로 휠 */
  int8_t  pan;           /**< 가로 휠 (AC Pan) */
} hid_mouse_report_t;

/**
 * 게임패드. 필드 순서와 이름은 TinyUSB `hid_gamepad_report_t` 그대로다 —
 * 상류 예제가 지정 초기화(`.buttons = ...`)로 쓰므로 바꾸면 깨진다 (R12).
 * 패킹해서 11바이트. 아래 리포트 맵(6×8 + 8 + 32 비트)과 반드시 맞아야 한다.
 */
typedef struct __attribute__((packed)) {
  int8_t   x;          /**< 왼쪽 스틱 X */
  int8_t   y;          /**< 왼쪽 스틱 Y */
  int8_t   z;          /**< 오른쪽 스틱 X (Z) */
  int8_t   rz;         /**< 오른쪽 스틱 Y (Rz) */
  int8_t   rx;         /**< 왼쪽 트리거 (Rx) */
  int8_t   ry;         /**< 오른쪽 트리거 (Ry) */
  uint8_t  hat;        /**< GAMEPAD_HAT_* — 방향 패드 */
  uint32_t buttons;    /**< GAMEPAD_BUTTON_* 비트합 */
} hid_gamepad_report_t;

/** 방향 패드. 0 이 중립이고 1~8 이 시계 방향이다 (리포트 맵의 논리 범위 1~8). */
enum {
  GAMEPAD_HAT_CENTERED   = 0,
  GAMEPAD_HAT_UP         = 1,
  GAMEPAD_HAT_UP_RIGHT   = 2,
  GAMEPAD_HAT_RIGHT      = 3,
  GAMEPAD_HAT_DOWN_RIGHT = 4,
  GAMEPAD_HAT_DOWN       = 5,
  GAMEPAD_HAT_DOWN_LEFT  = 6,
  GAMEPAD_HAT_LEFT       = 7,
  GAMEPAD_HAT_UP_LEFT    = 8,
};

/** 버튼 32개. 상류 예제는 `GAMEPAD_BUTTON_0` 처럼 이름으로 쓴다. */
enum {
  GAMEPAD_BUTTON_0  = (1UL <<  0), GAMEPAD_BUTTON_1  = (1UL <<  1),
  GAMEPAD_BUTTON_2  = (1UL <<  2), GAMEPAD_BUTTON_3  = (1UL <<  3),
  GAMEPAD_BUTTON_4  = (1UL <<  4), GAMEPAD_BUTTON_5  = (1UL <<  5),
  GAMEPAD_BUTTON_6  = (1UL <<  6), GAMEPAD_BUTTON_7  = (1UL <<  7),
  GAMEPAD_BUTTON_8  = (1UL <<  8), GAMEPAD_BUTTON_9  = (1UL <<  9),
  GAMEPAD_BUTTON_10 = (1UL << 10), GAMEPAD_BUTTON_11 = (1UL << 11),
  GAMEPAD_BUTTON_12 = (1UL << 12), GAMEPAD_BUTTON_13 = (1UL << 13),
  GAMEPAD_BUTTON_14 = (1UL << 14), GAMEPAD_BUTTON_15 = (1UL << 15),
  GAMEPAD_BUTTON_16 = (1UL << 16), GAMEPAD_BUTTON_17 = (1UL << 17),
  GAMEPAD_BUTTON_18 = (1UL << 18), GAMEPAD_BUTTON_19 = (1UL << 19),
  GAMEPAD_BUTTON_20 = (1UL << 20), GAMEPAD_BUTTON_21 = (1UL << 21),
  GAMEPAD_BUTTON_22 = (1UL << 22), GAMEPAD_BUTTON_23 = (1UL << 23),
  GAMEPAD_BUTTON_24 = (1UL << 24), GAMEPAD_BUTTON_25 = (1UL << 25),
  GAMEPAD_BUTTON_26 = (1UL << 26), GAMEPAD_BUTTON_27 = (1UL << 27),
  GAMEPAD_BUTTON_28 = (1UL << 28), GAMEPAD_BUTTON_29 = (1UL << 29),
  GAMEPAD_BUTTON_30 = (1UL << 30), GAMEPAD_BUTTON_31 = (1UL << 31),
};

/* ── 수정자 / 버튼 ─────────────────────────────────────────────────── */

enum {
  KEYBOARD_MODIFIER_LEFTCTRL   = 1u << 0,
  KEYBOARD_MODIFIER_LEFTSHIFT  = 1u << 1,
  KEYBOARD_MODIFIER_LEFTALT    = 1u << 2,
  KEYBOARD_MODIFIER_LEFTGUI    = 1u << 3,
  KEYBOARD_MODIFIER_RIGHTCTRL  = 1u << 4,
  KEYBOARD_MODIFIER_RIGHTSHIFT = 1u << 5,
  KEYBOARD_MODIFIER_RIGHTALT   = 1u << 6,
  KEYBOARD_MODIFIER_RIGHTGUI   = 1u << 7,
};

enum {
  MOUSE_BUTTON_LEFT     = 1u << 0,
  MOUSE_BUTTON_RIGHT    = 1u << 1,
  MOUSE_BUTTON_MIDDLE   = 1u << 2,
  MOUSE_BUTTON_BACKWARD = 1u << 3,
  MOUSE_BUTTON_FORWARD  = 1u << 4,
};

/* ── 자주 쓰는 키코드 (HID Usage Page 0x07) ────────────────────────── */

enum {
  HID_KEY_NONE = 0x00,
  HID_KEY_A = 0x04, HID_KEY_B, HID_KEY_C, HID_KEY_D, HID_KEY_E, HID_KEY_F,
  HID_KEY_G, HID_KEY_H, HID_KEY_I, HID_KEY_J, HID_KEY_K, HID_KEY_L,
  HID_KEY_M, HID_KEY_N, HID_KEY_O, HID_KEY_P, HID_KEY_Q, HID_KEY_R,
  HID_KEY_S, HID_KEY_T, HID_KEY_U, HID_KEY_V, HID_KEY_W, HID_KEY_X,
  HID_KEY_Y, HID_KEY_Z,
  HID_KEY_1 = 0x1E, HID_KEY_2, HID_KEY_3, HID_KEY_4, HID_KEY_5,
  HID_KEY_6, HID_KEY_7, HID_KEY_8, HID_KEY_9, HID_KEY_0,
  HID_KEY_ENTER = 0x28, HID_KEY_ESCAPE, HID_KEY_BACKSPACE, HID_KEY_TAB,
  HID_KEY_SPACE = 0x2C, HID_KEY_MINUS, HID_KEY_EQUAL, HID_KEY_BRACKET_LEFT,
  HID_KEY_BRACKET_RIGHT, HID_KEY_BACKSLASH, HID_KEY_EUROPE_1, HID_KEY_SEMICOLON,
  HID_KEY_APOSTROPHE, HID_KEY_GRAVE, HID_KEY_COMMA, HID_KEY_PERIOD, HID_KEY_SLASH,
  HID_KEY_CAPS_LOCK = 0x39,
  HID_KEY_F1 = 0x3A, HID_KEY_F2, HID_KEY_F3, HID_KEY_F4, HID_KEY_F5, HID_KEY_F6,
  HID_KEY_F7, HID_KEY_F8, HID_KEY_F9, HID_KEY_F10, HID_KEY_F11, HID_KEY_F12,
  HID_KEY_PRINT_SCREEN = 0x46, HID_KEY_SCROLL_LOCK, HID_KEY_PAUSE,
  HID_KEY_INSERT = 0x49, HID_KEY_HOME, HID_KEY_PAGE_UP, HID_KEY_DELETE,
  HID_KEY_END = 0x4D, HID_KEY_PAGE_DOWN,
  HID_KEY_ARROW_RIGHT = 0x4F, HID_KEY_ARROW_LEFT, HID_KEY_ARROW_DOWN, HID_KEY_ARROW_UP,
  /* 숫자 키패드 */
  HID_KEY_NUM_LOCK = 0x53, HID_KEY_KEYPAD_DIVIDE, HID_KEY_KEYPAD_MULTIPLY,
  HID_KEY_KEYPAD_SUBTRACT, HID_KEY_KEYPAD_ADD, HID_KEY_KEYPAD_ENTER,
  HID_KEY_KEYPAD_1 = 0x59, HID_KEY_KEYPAD_2, HID_KEY_KEYPAD_3, HID_KEY_KEYPAD_4,
  HID_KEY_KEYPAD_5, HID_KEY_KEYPAD_6, HID_KEY_KEYPAD_7, HID_KEY_KEYPAD_8,
  HID_KEY_KEYPAD_9, HID_KEY_KEYPAD_0, HID_KEY_KEYPAD_DECIMAL,
};

/* ── 소비자(미디어) 키 — Usage Page 0x0C ───────────────────────────── */

enum {
  HID_USAGE_CONSUMER_PLAY_PAUSE   = 0x00CD,
  HID_USAGE_CONSUMER_SCAN_NEXT    = 0x00B5,
  HID_USAGE_CONSUMER_SCAN_PREVIOUS= 0x00B6,
  HID_USAGE_CONSUMER_STOP         = 0x00B7,
  HID_USAGE_CONSUMER_MUTE         = 0x00E2,
  HID_USAGE_CONSUMER_VOLUME_INCREMENT = 0x00E9,
  HID_USAGE_CONSUMER_VOLUME_DECREMENT = 0x00EA,
};

/* ── 리포트 ID ─────────────────────────────────────────────────────── */

enum {
  REPORT_ID_KEYBOARD = 1,
  REPORT_ID_CONSUMER_CONTROL,
  REPORT_ID_MOUSE,
};

/**
 * ASCII -> {수정자 필요 여부, 키코드}. US 배열 기준.
 * 인덱스가 ASCII 코드다. 첫 값이 1 이면 Shift 가 필요하다.
 */
extern const uint8_t hid_ascii_to_keycode[128][2];

/** 키보드 + 소비자 + 마우스 합본 리포트 맵. */
extern const uint8_t hid_report_descriptor[];
extern const uint16_t hid_report_descriptor_len;

#endif
