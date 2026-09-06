/*
 * IEEE11073float.h — IEEE-11073 32비트 FLOAT 변환
 * baram-nrf54-arduino / SPDX-License-Identifier: MIT
 *
 * Health Thermometer 등 의료 프로파일이 온도를 이 형식으로 싣는다.
 * Adafruit Bluefruit52Lib 의 같은 이름 헤더와 API 를 맞췄다.
 *
 * 형식: 32비트. 상위 8비트가 부호 있는 10의 지수, 하위 24비트가 부호 있는
 *       가수(mantissa). 값 = mantissa * 10^exponent.
 */
#ifndef _IEEE11073FLOAT_H_
#define _IEEE11073FLOAT_H_

#include <stdint.h>
#include <math.h>

/**
 * float 을 IEEE-11073 32비트 FLOAT 로 바꿔 리틀엔디언 4바이트로 쓴다.
 *
 * 소수 둘째 자리까지 유지한다 (지수 -2). 체온계 해상도로 충분하고,
 * 지수를 값마다 바꾸면 상대가 읽을 때 정밀도가 들쭉날쭉해진다.
 */
static inline void float2IEEE11073(float data, uint8_t *buf)
{
  int32_t mantissa = (int32_t) roundf(data * 100.0f);
  const int8_t exponent = -2;

  /* 가수는 24비트다. 넘으면 표현할 수 없으므로 잘라낸다. */
  if (mantissa >  0x007FFFFF) mantissa =  0x007FFFFF;
  if (mantissa < -0x00800000) mantissa = -0x00800000;

  uint32_t v = ((uint32_t) mantissa) & 0x00FFFFFFUL;
  v |= ((uint32_t) (uint8_t) exponent) << 24;

  buf[0] = (uint8_t) (v      );
  buf[1] = (uint8_t) (v >>  8);
  buf[2] = (uint8_t) (v >> 16);
  buf[3] = (uint8_t) (v >> 24);
}

#endif
