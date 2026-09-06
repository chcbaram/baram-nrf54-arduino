/*
 * baram-nrf54l-arduino — nRF54L05 + SoftDevice S145 v10.0.1
 * SPDX-License-Identifier: MIT
 *
 * 메모리 배치 근거: sdk-nrf-bm v2.0.1
 *   boards/nordic/bm_nrf54l15dk/bm_nrf54l15dk_nrf54l05_cpuapp_s145_softdevice.dts
 * 칩 용량 근거: MDK 9.0.2 nrf54l05_xxaa_application_memory.h
 *   NRF_MEMORY_FLASH_SIZE 0x0007D000 (500 KB) / NRF_MEMORY_RAM_SIZE 0x00018000 (96 KB)
 * 상세: docs/MEMORY-MAP.md
 *
 * ⚠ nRF54L05 는 nRF54L15 와 같은 다이를 비닝한 것이다. 그래서 사양을 넘는
 *   RRAM/RAM 주소를 읽고 써도 실기에서는 "동작해 버린다". nRF54L15 링커
 *   스크립트로 L05 를 구워도 blink 가 도는 이유가 이것이며, 사양 밖이므로
 *   절대 그렇게 쓰지 않는다. 칩 구분은 FICR INFO.PART(0x00FFC31C) 로 한다.
 *     0x00054B05 = nRF54L05,  0x00054B15 = nRF54L15
 *
 *   RRAM 전체 배치 (500 KB):
 *     0x00000000  application    354 KB (0x58800)    <- 이 스크립트
 *     0x00058800  peer_manager     4 KB
 *     0x00059800  storage0         4 KB
 *     0x0005A800  SoftDevice S145 137 KB (0x22400)
 *     0x0007D000  (RRAM 끝)
 *
 *   SoftDevice hex 는 SoC 별로 재배치된 별도 빌드다. L05 용은
 *   s145_nrf54l05_10.0.1_softdevice.hex 이며 0x0005A800 에 로드된다
 *   (L15 용은 0x0015A800). 크기는 137 KB 로 동일하지만 서로 호환되지 않는다.
 *
 *   RAM 전체 배치 (96 KB):
 *     0x20000000  SoftDevice    0x04780
 *     0x20004780  application   0x13880             <- 이 스크립트
 *     0x20018000  (RAM 끝)
 *
 *   위 DTS 의 app_ram 은 DT_SIZE_K(78) = 0x13800 으로 적혀 있어 상단 128 바이트가
 *   남는다. 이는 디바이스 트리가 크기를 K 단위로만 표기하기 때문에 생긴 표기상의
 *   내림이지 예약 영역이 아니다. 여기서는 RAM 끝(0x20018000)까지 전부 쓴다.
 */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x58800
  RAM   (rwx) : ORIGIN = 0x20004780, LENGTH = 0x13880
}

/*
 * 스택/힙 정책은 nrf54l15_s145_v10.ld 와 동일하다. 두 값 모두 platform.txt 에서
 * -D__STACK_SIZE / -D__HEAP_SIZE 로 넘어온다.
 */

/*
 * SoftDevice 파티션 시작 주소.
 * ble/sd_event_pump.c 가 softdevice_vector_forward_address 에 넣는 값이고,
 * SoftDevice hex 가 실제로 로드되는 주소와 같아야 한다 (nRF54L05).
 * hex 안에 절대 주소로 박혀 있어 옮길 수 없다 (CLAUDE.md §7 F11).
 */
__softdevice_start__ = 0x0005A800;

/* 애플리케이션 RAM 시작. sd_ble_enable() 에 넘긴다. */
__app_ram_start__ = ORIGIN(RAM);

INCLUDE "nrf_common.ld"
