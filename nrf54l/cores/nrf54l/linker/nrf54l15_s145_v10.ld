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

/*
 * SoftDevice RAM 예약 — 0x20000000 ~ 0x20008000 (32 KB)
 *
 * DTS 기본값은 0x4780(18.25 KB)이고 그것도 링크 1개에는 충분하다
 * (실측 요구 0x20003750). 넉넉히 잡는 이유는 **동시 연결** 때문이다.
 *
 * 실측 (MTU 247, XIAO nRF54L15, notify 큐 1):
 *   링크 1개  0x20003750      링크 4개  0x200065F8
 *   링크 2개  0x200046D8      링크 5개  0x20007590
 *   링크 3개  0x20005668
 * 링크당 약 3980 B 다. MTU 를 줄이면 링크당 비용도 준다 (MTU 23 이면 약 1950 B).
 *
 * ⚠ notify 큐 깊이도 같은 RAM 을 먹는다. 한 칸이 **링크당 약 251 B**(≈MTU)다.
 *   그래서 링크 수와 큐 깊이는 맞바꾸는 관계이고, 둘을 따로 정할 수 없다.
 *
 * 지금 구성은 **peripheral 4 + central 1 + 큐 3** 이고, 필요량은 0x20007F48 이다 (실측).
 * 0x20008000 은 그것을 담고 184 B 가 남는다.
 *
 * 참고 실측 (MTU 247, 큐 3):
 *   periph 4 + central 0   0x20006DD8
 *   periph 3 + central 1   0x20006DC0   (central 이 24 B 싸다)
 *   periph 4 + central 1   0x20007F48   <- 이 값이 기준
 * 비용은 역할이 아니라 링크 수에 붙는다. 자세한 표는 docs/MEMORY-MAP.md.
 *
 * 참고로 Adafruit nRF52840 코어는 0x20006000(24 KB)을 잡는데,
 * 그 값이면 이 칩에서 링크 3개까지다.
 *
 * ⚠ 설정을 바꿔 RAM 이 모자라면 sd_ble_enable() 이 **필요한 정확한 주소**를
 *   돌려준다. sdCfgResults() 로 읽어 이 값과 아래 LENGTH,
 *   그리고 boards.txt 의 upload.maximum_data_size 를 함께 고쳐라.
 */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x158800
  RAM   (rwx) : ORIGIN = 0x20008000, LENGTH = 0x38000
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
/*
 * 본딩 키 저장 파티션 (peer_manager). 앱 파티션 **밖**이라 앱을 다시 구워도
 * 지워지지 않는다.
 *
 * ⚠ RRAM 은 erase 가 없고 **제자리에서 덮어쓸 수 있다** — 0 -> 1 전이도 된다
 *   (실기 확인: 00000000 -> FFFFFFFF). NOR 플래시처럼 append-log 를 만들 필요가
 *   없고 고정 슬롯을 그냥 덮어쓰면 된다.
 *
 * ⚠ SoftDevice 가 켜져 있을 때는 **`sd_flash_write()` 로만** 써야 한다.
 *   라디오와 겹치지 않게 SoftDevice 가 시점을 잡아 준다. 이 주소가 그 API 의
 *   "application flash area" 안에 드는지는 실기로 확인했다 (성공).
 */
__bond_storage_start__ = 0x00158800;
__bond_storage_size__  = 0x1000;

__softdevice_start__ = 0x0015A800;

/* 애플리케이션 RAM 시작. sd_ble_enable() 에 넘긴다. */
__app_ram_start__ = ORIGIN(RAM);

INCLUDE "nrf_common.ld"
