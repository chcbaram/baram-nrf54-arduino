/*
 * syscalls.c — newlib 하위 훅
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>

#include "nrf.h"

/* 링커 스크립트(MDK nrf_common.ld)가 주는 심볼 */
extern char __HeapBase;    /* .heap 블록 시작 */
extern char __StackLimit;  /* 스택 하단 */

/*
 * ── 힙 정책 ───────────────────────────────────────────────────────────
 *
 * platform.txt 가 __HEAP_SIZE=0 을 넘기므로 MDK 의 고정 .heap 블록은
 * 비어 있다. 그래서 _sbrk 가 __HeapBase 부터 __StackLimit 직전까지
 * 남은 RAM 전부를 힙으로 쓴다.
 *
 * Adafruit nRF52 코어와 같은 정책이다: MSP(하드웨어 스택)는 ISR 과
 * SoftDevice 전용으로 작게(2KB) 두고, FreeRTOS 태스크 스택은 전부
 * 힙에서 잡는다(heap_3 = newlib malloc). 그래야 태스크 개수와 크기를
 * 링커 스크립트 수정 없이 바꿀 수 있다.
 *
 * ⚠ 스택과 힙이 만나는 것을 여기서만 막는다. 넘치면 -1 을 돌려
 *   malloc 이 실패하고, vApplicationMallocFailedHook 이 잡는다.
 */
void *_sbrk(int incr)
{
    static char *heap_end = NULL;
    char *prev;

    if (heap_end == NULL) {
        heap_end = &__HeapBase;
    }

    prev = heap_end;

    if ((heap_end + incr) > &__StackLimit) {
        errno = ENOMEM;
        return (void *) -1;
    }

    heap_end += incr;
    return (void *) prev;
}

/* 쓰지 않는 newlib 훅들. --specs=nosys.specs 가 주는 것보다
 * 여기서 명시하는 편이 링크 경고가 없다. */
int  _close(int file)                          { (void)file; return -1; }
int  _fstat(int file, struct stat *st)         { (void)file; st->st_mode = S_IFCHR; return 0; }
int  _isatty(int file)                         { (void)file; return 1; }
int  _lseek(int file, int ptr, int dir)        { (void)file; (void)ptr; (void)dir; return 0; }
int  _read(int file, char *ptr, int len)       { (void)file; (void)ptr; (void)len; return 0; }
int  _getpid(void)                             { return 1; }
int  _kill(int pid, int sig)                   { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int status)                         { (void)status; NVIC_SystemReset(); while (1) {} }

/*
 * printf 계열의 출력처. 기본은 Serial 이다.
 * Uart.cpp 가 nrf54l_serial_write_bytes() 를 제공한다.
 */
__attribute__((weak)) int nrf54l_serial_write_bytes(const char *buf, int len)
{
    (void) buf;
    return len;
}

int _write(int file, char *ptr, int len)
{
    (void) file;
    return nrf54l_serial_write_bytes(ptr, len);
}
