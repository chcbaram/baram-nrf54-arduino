/*
 * nrfx 로그 — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * nrfx 내부 로그는 쓰지 않는다. 전부 no-op.
 */
#ifndef NRFX_LOG_H__
#define NRFX_LOG_H__

#define NRFX_LOG_ERROR(...)
#define NRFX_LOG_WARNING(...)
#define NRFX_LOG_INFO(...)
#define NRFX_LOG_DEBUG(...)

#define NRFX_LOG_HEXDUMP_ERROR(p_memory, length)
#define NRFX_LOG_HEXDUMP_WARNING(p_memory, length)
#define NRFX_LOG_HEXDUMP_INFO(p_memory, length)
#define NRFX_LOG_HEXDUMP_DEBUG(p_memory, length)

#define NRFX_LOG_ERROR_STRING_GET(error_code) ""

#endif /* NRFX_LOG_H__ */
