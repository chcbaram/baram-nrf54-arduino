/*
 * SoftDevice 지원 상태 — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * nRF54L 은 애플리케이션이 벡터 테이블을 소유하고 SoftDevice 로 예외를
 * 포워딩한다 (CLAUDE.md §7 F1). 그 포워딩에 필요한 상태를 담는다.
 *
 * ⚠ 지금은 M1 단계라 SoftDevice 를 아직 켜지 않는다.
 *   M3 에서 sdk-nrf-bm 의 subsys/softdevice_handler 를 이식하면서
 *   이 파일은 실제 핸들러(nrf_sdh.c / irq_connect.c 이식본)로 대체된다.
 *   그때까지는 심볼만 제공해 링크가 되게 한다.
 */

#include <stdint.h>

/*
 * SoftDevice 벡터 테이블의 베이스 주소.
 *
 * SoftDevice 가 활성화되면 sd_softdevice_enable() 전에
 * 이 값을 SoftDevice 파티션 시작 주소(0x0015A800, docs/MEMORY-MAP.md)로
 * 채워야 한다. sd_svc_dispatch.S 가 여기서 읽어
 * [base + NRF_SD_ISR_OFFSET_SVC] 로 점프한다.
 *
 * 0 이면 SoftDevice 가 없다는 뜻이다. 이 상태에서 SVC >= 0x10 이
 * 발생하면 널 포인터로 점프해 HardFault 가 난다 — SoftDevice 를 켜지
 * 않고 sd_* API 를 부른 것이므로 그게 맞는 동작이다.
 */
uint32_t softdevice_vector_forward_address = 0;

/*
 * IRQ 포워딩 활성 플래그. sdk-nrf-bm 의
 * irq_forwarding_enabled_magic_number_holder 와 같은 역할이다.
 * M3 에서 ConsumeOrForwardIRQ 계열을 이식할 때 쓴다.
 */
uint32_t irq_forwarding_enabled_magic_number_holder = 0;
