/*
 * main.cpp — 부팅과 loop 태스크
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 구조는 Adafruit_nRF52_Arduino cores/nRF5/main.cpp (BSD-3-Clause) 를
 * 그대로 따랐다. setup()/loop() 이 FreeRTOS 태스크 위에서 도는 것이
 * 사용자에게 노출된 계약이다 (CLAUDE.md §6).
 */

#include "Arduino.h"

/*
 * loop 태스크 스택. 바이트가 아니라 워드 단위다.
 * Adafruit 과 같은 값(256*4 워드 = 4KB)을 쓴다. 스케치가 이 크기를
 * 전제하고 지역 버퍼를 잡는 경우가 있어 줄이지 않는다.
 */
#define LOOP_STACK_SZ   (256 * 4)

static TaskHandle_t _loopHandle = NULL;

extern "C" void loop_task(void *arg)
{
  (void) arg;

  setup();

  while (1)
  {
    loop();
    yield();
  }
}

/* variant 가 재정의한다. */
extern "C" void initVariant(void) __attribute__((weak));
extern "C" void initVariant(void) { }

int main(void)
{
  init();
  initVariant();

  xTaskCreate(loop_task, "loop", LOOP_STACK_SZ, NULL, TASK_PRIO_LOW, &_loopHandle);

  vTaskStartScheduler();

  /* 여기 오면 스케줄러가 죽은 것이다. 힙 부족이 가장 흔한 원인. */
  NVIC_SystemReset();
  return 0;
}

/*
 * loop() 태스크를 재우고 깨운다.
 * 저전력 스케치에서 인터럽트가 올 때까지 loop 를 멈추는 데 쓴다
 * (CLAUDE.md §6).
 */
void suspendLoop(void)
{
  if (_loopHandle) vTaskSuspend(_loopHandle);
}

void resumeLoop(void)
{
  if (_loopHandle) {
    if (xPortIsInsideInterrupt()) {
      BaseType_t woken = pdFALSE;
      xTaskResumeFromISR(_loopHandle);
      (void) woken;
    } else {
      vTaskResume(_loopHandle);
    }
  }
}
