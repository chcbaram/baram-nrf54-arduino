/*
 * Adafruit_LittleFS.h — 안내용 헤더
 * baram-nrf54l-arduino / SPDX-License-Identifier: MIT
 *
 * 이 코어에는 파일시스템이 없다. 그런데 Adafruit 예제들이 이 헤더를
 * include 하고 있어서, 아무것도 없으면 "No such file or directory" 만 나온다.
 * 무엇을 해야 하는지 알려주려고 둔 헤더다.
 */
#ifndef _ADAFRUIT_LITTLEFS_H_
#define _ADAFRUIT_LITTLEFS_H_

#error "This core has no filesystem. Delete the two lines #include <Adafruit_LittleFS.h> and #include <InternalFileSystem.h> from the sketch. Adafruit examples include them only so the bonding code gets linked; this core stores bonds directly in an RRAM partition, so they are not needed. No example calls the filesystem API itself (CLAUDE.md 8.1)."

#endif
