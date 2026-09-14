/*
 * baram-nrf54l-arduino — nRF54LM20A + SoftDevice S145 v10.0.1
 * SPDX-License-Identifier: MIT
 *
 * 메모리 배치 근거: sdk-nrf-bm v2.0.1
 *   boards/nordic/bm_nrf54lm20dk/bm_nrf54lm20dk_nrf54lm20a_cpuapp_s145_softdevice.dts
 * 칩 용량 근거: MDK nrf54lm20a_xxaa_application_memory.h
 *   NRF_MEMORY_FLASH_SIZE 0x001FD000 (2036 KB) — DTS 의 cpuapp_rram 과 일치
 * 상세: docs/MEMORY-MAP.md
 *
 *   RRAM 전체 배치 (2036 KB):
 *     0x00000000  application   1890 KB (0x1D8800)   <- 이 스크립트
 *     0x001D8800  peer_manager     4 KB
 *     0x001D9800  storage0         4 KB
 *     0x001DA800  SoftDevice S145 137 KB (0x22400)
 *     0x001FD000  (RRAM 끝)
 *
 *   SoftDevice hex 는 SoC 별로 재배치된 별도 빌드다. LM20 용은
 *   s145_nrf54lm20_10.0.1_softdevice.hex 이고, 실측 로드 범위가
 *   0x001DA800 ~ 0x001FC57C 로 위 파티션과 맞는다. L15 용과 호환되지 않는다.
 *
 *   RAM 전체 배치:
 *     0x20000000  SoftDevice    0x08000
 *     0x20008000  application   0x77D40              <- 이 스크립트
 *     0x2007FD40  (RAM 끝)
 *
 * ⚠ **RAM 은 512 KB 가 아니라 0x7FD40 (511 KB + 0x140) 이다. 1 KB 단위로 떨어지지 않는다.**
 *   DTS 원문: "As the total size of SRAM is not 1kB aligned, 0x140 bytes are added
 *   to the 511 kB" — cpuapp_sram 이 0x20000080 부터 511K - 0x80 + 0x140 이라
 *   끝이 0x2007FD40 이다.
 *
 *   처음에 이것을 "대략 512 KB" 로 읽고 끝을 0x20080000 으로 잡았다가 실기에서
 *   **부팅 첫 명령에 HardFault** 가 났다 (실측 CFSR 0x9201 = STKERR + BFARVALID,
 *   BFAR 0x2007FFF8 — 초기 SP 에서 첫 push 가 없는 주소에 썼다). 증상이 LED 가
 *   안 켜지는 것뿐이라 variant 문제로 오진하기 쉽다.
 *
 *   다른 출처는 믿지 마라:
 *     FICR INFO.RAM  0x200 (512 KB) — KB 단위로 올림된 값이다
 *     MDK  NRF_MEMORY_RAM_SIZE 0x40000 (256 KB) — L15 에서도 틀렸던 값이다
 *
 * SoftDevice RAM 예약 32 KB 는 peripheral 4 + central 1 + 큐 3 의 요구량을 담는다.
 * **LM20 hex 로도 실측이 0x20007F48 로 L15·L05 와 같았다** (XIAO nRF54LM20A,
 * sdCfgResults(), MTU 247) — 184 B 가 남는다. SoftDevice RAM 요구량은 SoC 와 무관하다.
 * 설정을 키워 모자라면 sd_ble_enable() 이 필요한 주소를 돌려준다.
 *
 * DTS 는 0x20000000 ~ 0x20000080 을 KMU 예약으로 뗀다. 우리는 KMU 를 쓰지 않고
 * (CRACEN 은 TRNG 만), 그 영역은 SoftDevice 예약 안에 들어 있다 — L15 와 같다.
 */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x1D8800
  RAM   (rwx) : ORIGIN = 0x20008000, LENGTH = 0x77D40
}

/*
 * 부트로더가 없는 M1~M3 구성에서도 SoftDevice 영역을 침범하지 않도록
 * FLASH 길이를 앱 파티션으로 못박아 둔다.
 */

/*
 * 스택/힙 정책 (Adafruit nRF52 코어와 동일)
 *   __STACK_SIZE : MSP. ISR과 SoftDevice만 쓴다. FreeRTOS 태스크는 힙에서 잡는다
 *   __HEAP_SIZE  : 0. MDK의 고정 .heap 블록을 비우고, _sbrk가
 *                  __HeapBase ~ __StackLimit 사이를 전부 힙으로 쓴다
 * 두 값 모두 platform.txt에서 -D로 넘긴다.
 */

/*
 * 본딩 키 저장 파티션 (peer_manager). 앱 파티션 **밖**이라 앱을 다시 구워도
 * 지워지지 않는다. RRAM 이라 고정 슬롯을 제자리에서 덮어쓴다.
 * SoftDevice 가 켜져 있을 때는 sd_flash_write() 로만 쓴다.
 */
__bond_storage_start__ = 0x001D8800;
__bond_storage_size__  = 0x1000;

/*
 * SoftDevice 파티션 시작 주소. ble/sd_event_pump.c 가
 * softdevice_vector_forward_address 에 넣는다. hex 에 절대 주소로 박혀 있다.
 */
__softdevice_start__ = 0x001DA800;

/* 애플리케이션 RAM 시작. sd_ble_enable() 에 넘긴다. */
__app_ram_start__ = ORIGIN(RAM);

INCLUDE "nrf_common.ld"
