/*
  Copyright (c) 2014 Arduino.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdlib.h>
#include "rtos.h"

#include <errno.h>
#include <sys/stat.h>
#include <malloc.h>

__attribute__((weak))
void *operator new(size_t size) {
  return rtos_malloc(size);
}

__attribute__((weak))
void *operator new[](size_t size) {
  return rtos_malloc(size);
}

__attribute__((weak))
void operator delete(void * ptr) {
  rtos_free(ptr);
}

__attribute__((weak))
void operator delete[](void * ptr) {
  rtos_free(ptr);
}

__attribute__((weak))
void operator delete(void * ptr, unsigned int) {
  rtos_free(ptr);
}

__attribute__((weak))
void operator delete[](void * ptr, unsigned int) {
  rtos_free(ptr);
}


/*
 * [baram-nrf54l-arduino] 여기 있던 _sbrk 는 제거했다.
 *
 * 원본은 __HeapBase ~ __HeapLimit 사이만 힙으로 썼는데, 우리는
 * platform.txt 에서 __HEAP_SIZE=0 을 넘겨 MDK 의 고정 .heap 블록을
 * 비워 두므로 그 구간이 0 바이트다. 그대로 두면 malloc 이 항상 실패한다.
 *
 * 대신 cores/nrf54l/syscalls.c 의 _sbrk 가 __HeapBase 부터 __StackLimit
 * 직전까지 남은 RAM 전부를 힙으로 쓴다. 태스크 스택도 거기서 나온다.
 */
