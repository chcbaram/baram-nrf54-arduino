/*
 * rtos.h — SchedulerRTOS
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit_nRF52_Arduino cores/nRF5/rtos.h
 * (BSD-3-Clause, Copyright (c) 2018 Adafruit Industries) 와
 * **동일한 API** 다. 스케치 호환의 핵심이므로 시그니처를 바꾸지 마라
 * (CLAUDE.md §8).
 *
 *   void setup() { Scheduler.startLoop(loop2); }
 *   void loop()  { digitalToggle(LED_RED);  delay(1000); }
 *   void loop2() { digitalToggle(LED_BLUE); delay(500);  }
 */
#ifndef RTOS_H_
#define RTOS_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#define DELAY_FOREVER   portMAX_DELAY

/*
 * 태스크 우선순위. FreeRTOSConfig.h 의 configMAX_PRIORITIES = 5 와 맞춰져
 * 있다. Adafruit 과 값이 같아야 스케치가 그대로 돈다.
 *
 * ⚠ 이건 FreeRTOS 태스크 우선순위이지 NVIC 인터럽트 우선순위가 아니다.
 *   혼동하지 마라. 인터럽트 쪽은 CLAUDE.md §7 F2 를 볼 것.
 */
enum
{
  TASK_PRIO_LOWEST  = 0, // Idle task. 쓰지 말 것
  TASK_PRIO_LOW     = 1, // loop()
  TASK_PRIO_NORMAL  = 2, // Timer task, Callback task
  TASK_PRIO_HIGH    = 3, // Bluefruit task (M3)
  TASK_PRIO_HIGHEST = 4,
};

#ifndef ms2tick
  #define ms2tick     pdMS_TO_TICKS
#endif
#ifndef tick2ms
  #define tick2ms(tck)  ( ( ((uint64_t)(tck)) * 1000)    / configTICK_RATE_HZ )
#endif
#define tick2us(tck)    ( ( ((uint64_t)(tck)) * 1000000) / configTICK_RATE_HZ )

/* heap_3 을 쓰므로 malloc/free 가 곧 thread-safe 하다. */
#define rtos_malloc   malloc
#define rtos_free     free

#ifdef __cplusplus

#define SCHEDULER_STACK_SIZE_DFLT   (512*2)

class SchedulerRTOS
{
public:
  typedef void (*taskfunc_t)(void);

  SchedulerRTOS(void);

  bool startLoop(taskfunc_t task,
                 uint32_t stack_size = SCHEDULER_STACK_SIZE_DFLT,
                 uint32_t prio       = TASK_PRIO_LOW,
                 const char* name    = NULL);
};

extern SchedulerRTOS Scheduler;

#endif // __cplusplus

#endif /* RTOS_H_ */
