/*
 * baram-nrf54l-arduino — nRF54L15 + SoftDevice S145 v10.0.1
 * SPDX-License-Identifier: MIT
 *
 * 메모리 배치 근거: sdk-nrf-bm v2.0.1
 *   boards/nordic/bm_nrf54l15dk/bm_nrf54l15dk_nrf54l15_cpuapp_s145_softdevice.dts
 * 상세: docs/MEMORY-MAP.md
 *
 * ⚠ nRF52와 배치가 반대다. 애플리케이션이 0x0에서 시작하고 벡터 테이블도
 *   애플리케이션 소유이며, SoftDevice가 RRAM 상단에 놓인다.
 *   따라서 VTOR 재배치가 필요 없다. Adafruit의 nrf52840_s140_v6.ld
 *   (FLASH ORIGIN = 0x26000)를 그대로 베끼면 안 된다. CLAUDE.md §7 F1 참조.
 *
 *   RRAM 전체 배치 (1.5MB):
 *     0x00000000  application   1378 KB (0x158800)   <- 이 스크립트
 *     0x00158800  peer_manager     4 KB
 *     0x00159800  storage0         4 KB
 *     0x0015A800  SoftDevice S145 137 KB (0x22400)
 *
 *   RAM 전체 배치 (256KB):
 *     0x20000000  SoftDevice    0x04780
 *     0x20004780  application   0x3B880              <- 이 스크립트
 */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x158800
  RAM   (rwx) : ORIGIN = 0x20004780, LENGTH = 0x3B880
}

/*
 * 부트로더가 없는 M1~M3 구성에서도 SoftDevice 영역을 침범하지 않도록
 * FLASH 길이를 앱 파티션으로 못박아 둔다. SoftDevice를 쓰지 않는 빌드라도
 * 같은 배치를 유지해서 M3 진입 시 재배치가 없게 한다.
 */

/*
 * 스택/힙 정책 (Adafruit nRF52 코어와 동일)
 *   __STACK_SIZE : MSP. ISR과 SoftDevice만 쓴다. FreeRTOS 태스크는 힙에서 잡는다
 *   __HEAP_SIZE  : 0. MDK의 고정 .heap 블록을 비우고, _sbrk가
 *                  __HeapBase ~ __StackLimit 사이를 전부 힙으로 쓴다
 * 두 값 모두 platform.txt에서 -D로 넘긴다.
 */

/*
 * SoftDevice 파티션 시작 주소.
 * ble/sd_event_pump.c 가 softdevice_vector_forward_address 에 넣는 값이고,
 * SoftDevice hex 가 실제로 로드되는 주소와 같아야 한다 (nRF54L15).
 * hex 안에 절대 주소로 박혀 있어 옮길 수 없다 (CLAUDE.md §7 F11).
 */
__softdevice_start__ = 0x0015A800;

/* 애플리케이션 RAM 시작. sd_ble_enable() 에 넘긴다. */
__app_ram_start__ = ORIGIN(RAM);

INCLUDE "nrf_common.ld"
