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

#error "이 코어에는 파일시스템이 없다. 스케치에서 #include <Adafruit_LittleFS.h> 와 #include <InternalFileSystem.h> 두 줄을 지워라. Adafruit 예제가 이 줄을 넣은 이유는 본딩 코드를 링크시키기 위해서인데, 이 코어는 본딩을 RRAM 파티션에 직접 저장하므로 필요 없다. 예제가 파일시스템 API 를 직접 쓰는 경우는 없다 (CLAUDE.md 8.1)."

#endif
