/*
 * rtos.cpp — SchedulerRTOS 구현 + FreeRTOS 훅
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 구조는 Adafruit_nRF52_Arduino cores/nRF5/rtos.cpp (BSD-3-Clause) 를 따랐다.
 */

#include "Arduino.h"
#include "rtos.h"

SchedulerRTOS Scheduler;

SchedulerRTOS::SchedulerRTOS(void)
{
}

/* 사용자 함수를 무한 루프로 감싼다. Arduino 의 loop() 와 같은 의미가 되게. */
static void _redirect_task(void *param)
{
  SchedulerRTOS::taskfunc_t taskfunc = (SchedulerRTOS::taskfunc_t) param;

  while (1)
  {
    taskfunc();
    yield();
  }
}

bool SchedulerRTOS::startLoop(taskfunc_t task, uint32_t stack_size, uint32_t prio, const char* name)
{
  /* stack_size 는 바이트 단위로 받아 워드로 바꾼다.
   * Adafruit 과 같은 규약이다. 여기서 4로 나누지 않으면 스택이 4배가 된다. */
  return xTaskCreate(_redirect_task,
                     name ? name : "loop",
                     stack_size / 4,
                     (void *) task,
                     prio,
                     NULL) == pdPASS;
}

extern "C"
{

/*
 * Arduino 의 yield(). delay() 안이나 대기 루프에서 불린다.
 * TinyUSB CDC flush 분기는 없다. nRF54L15 에는 USB 가 없다 (R10).
 */
void yield(void)
{
  taskYIELD();
}

/* ── FreeRTOS 훅 ───────────────────────────────────────────────────── */

void vApplicationMallocFailedHook(void)
{
  /* 힙 고갈. 태스크 스택도 힙에서 나오므로 startLoop 실패도 여기로 온다. */
  configASSERT(false);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void) xTask;
  (void) pcTaskName;
  configASSERT(false);
}

/*
 * configSUPPORT_STATIC_ALLOCATION = 1 이라 idle / timer 태스크 메모리를
 * 애플리케이션이 줘야 한다.
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
  static StaticTask_t idleTaskTCB;
  static StackType_t  idleTaskStack[configMINIMAL_STACK_SIZE];

  *ppxIdleTaskTCBBuffer   = &idleTaskTCB;
  *ppxIdleTaskStackBuffer = idleTaskStack;
  *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t  **ppxTimerTaskStackBuffer,
                                    uint32_t      *pulTimerTaskStackSize)
{
  static StaticTask_t timerTaskTCB;
  static StackType_t  timerTaskStack[configTIMER_TASK_STACK_DEPTH];

  *ppxTimerTaskTCBBuffer   = &timerTaskTCB;
  *ppxTimerTaskStackBuffer = timerTaskStack;
  *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

/* configASSERT 실패 지점. 디버거로 여기서 잡는다. */
void vAssertCalled(const char *file, int line)
{
  (void) file;
  (void) line;

  taskDISABLE_INTERRUPTS();
  while (1) { __asm__ volatile("nop"); }
}

/* nrfx_glue.h 의 NRFX_ASSERT 가 부른다. */
void nrf54l_assert_failed(const char *file, int line)
{
  vAssertCalled(file, line);
}

/* nrfx_glue.h 의 크리티컬 섹션 상태 (BASEPRI 중첩 카운터). */
uint32_t nrfx_glue_cs_nesting;
uint32_t nrfx_glue_cs_saved_basepri;

} // extern "C"
