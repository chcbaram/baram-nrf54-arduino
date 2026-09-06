# 메모리 맵 — 칩별 (nRF54L05 / nRF54L15)

**이 문서는 칩 단위다.** 보드가 아니라 실장 칩이 배치를 정한다.
보드별 핀맵은 `docs/boards/` 를 봐라.

| 보드 | 칩 | 아래 어느 절을 쓰나 |
|---|---|---|
| NU54-DK | nRF54L05 | nRF54L05 절 |
| NU54V-DK | nRF54L15 | nRF54L15 절 |
| XIAO nRF54L15 / Sense | nRF54L15 | nRF54L15 절 (NU54V-DK 와 동일) |

NU54-DK 와 NU54V-DK 는 회로도·핀맵이 동일하고 실장 모듈만 다르다.
메모리 배치만 다르므로 링커 스크립트와 SoftDevice hex 를 나눠 둔다.

| | NU54-DK | NU54V-DK |
|---|---|---|
| 칩 | **nRF54L05** | nRF54L15 |
| FQBN | `baram-nrf54:nrf54l:nu54dk` | `...:nu54vdk` |
| RRAM | 500 KB | 1.5 MB |
| RAM | 96 KB | 256 KB |
| 링커 | `nrf54l05_s145_v10.ld` | `nrf54l15_s145_v10.ld` |
| SD hex | `s145_nrf54l05_...` (@0x5A800) | `s145_nrf54l15_...` (@0x15A800) |

> ⚠ **nRF54L05 는 nRF54L15 와 같은 다이를 비닝한 것이다.**
> 사양 밖 RRAM/RAM 이 물리적으로 존재해서, L15 설정으로 L05 를 구워도
> 오류 없이 **그냥 동작해 버린다.** 실제로 M1 내내 그렇게 돌고 있었고
> 아무 증상도 없었다. 보드를 잘못 고르면 조용히 사양을 벗어나므로,
> 실장 칩은 FICR 로 확인한다:
>
> ```
> FICR INFO.PART     @ 0x00FFC31C   0x00054B05 = L05 / 0x00054B15 = L15
> FICR INFO.VARIANT  @ 0x00FFC320   ASCII (기능 variant + HW 버전)
> FICR INFO.PACKAGE  @ 0x00FFC324   ASCII (패키지 코드. 예 "CA")
> FICR INFO.RAM      @ 0x00FFC328   KB 단위. 0x60 = 96 KB(L05) / 0x100 = 256 KB(L15)
> FICR INFO.RRAM     @ 0x00FFC32C   KB 단위. 0x1F4 = 500 KB(L05) / 0x5F4 = 1524 KB(L15)
> ```
>
> ⚠ 이 오프셋은 원래 RAM 을 `0x00FFC324`, `CODESIZE` 를 `0x00FFC328` 로
> **4 바이트씩 앞당겨 적어 놓았다.** 그 자리는 실제로 `PACKAGE` / `RAM` 이라
> 그대로 읽으면 엉뚱한 값이 나온다. 근거는 MDK `NRF_FICR_INFO_Type`
> (FICR 베이스 `0x00FFC000` + `INFO` `0x300`) 이고, 위 값은 실기 판독으로 확인했다.
> 마지막 필드 이름도 `CODESIZE` 가 아니라 **`RRAM`** 이다.

---


출처: `nrfconnect/sdk-nrf-bm` v2.0.1
`boards/nordic/bm_nrf54l15dk/bm_nrf54l15dk_nrf54l15_cpuapp_s145_softdevice.dts`
(및 같은 디렉토리의 `..._s115_softdevice.dts`)

MCUboot 없음 / TrustZone 없음(secure-only, R5) 기준.

---

## NU54V-DK — nRF54L15 + S145 v10.0.1

### RRAM (1.5 MB = 0x17D000)

| 시작 | 크기 | 영역 | 비고 |
|---|---|---|---|
| `0x00000000` | 1378 KB (`0x158800`) | **application (slot0)** | 벡터 테이블 포함 |
| `0x00158800` | 4 KB (`0x1000`) | peer_manager | BLE 본딩 저장 (M3) |
| `0x00159800` | 4 KB (`0x1000`) | storage0 | 사용자 스토리지 |
| `0x0015A800` | 137 KB (`0x22400`) | **SoftDevice S145** | `0x17CC00`에서 끝 |

### RAM (256 KB = 0x40000)

| 시작 | 크기 | 영역 |
|---|---|---|
| `0x20000000` | `0x7800` (30 KB) | **SoftDevice** |
| `0x20007800` | `0x38800` (231,424 B) | **application** |

> **DTS 기본값(`0x4780`, 18.25 KB)보다 넉넉히 잡았다.** 이유는 **동시 연결**이다.
> SoftDevice 는 링크마다 RAM 을 더 쓰는데, 그 크기는 `sd_ble_enable()` 이
> 돌려주는 값으로만 알 수 있다.

**실측 (XIAO nRF54L15, MTU 247):**

| 동시 링크 | 필요한 앱 RAM 시작 | 링커 `0x20007800` 안에 |
|---|---|---|
| 1 | `0x20003750` | ✅ |
| 2 | `0x200046D8` | ✅ |
| 3 | `0x20005668` | ✅ |
| 4 | `0x200065F8` | ✅ |
| **5** | **`0x20007590`** | ✅ 여유 624 B |
| 6 | (미측정) | ❌ |

링크당 약 **3980 B** (MTU 247). MTU 를 줄이면 같이 준다 — MTU 23 이면 약 1950 B 라
같은 경계로 링크 8개도 들어간다. **연결 수와 MTU 는 맞바꾸는 관계다.**

참고로 Adafruit nRF52840 코어는 `0x20006000`(24 KB)을 잡는다. 그 값이면 링크 3개까지다.
전체 256 KB 중 30 KB 를 예약하므로 앱 RAM 손해는 5 % 다 (243,328 → 231,424 B).

⚠ 링크를 늘리면 라디오 시간을 나눠 쓰므로 **연결당 처리량이 떨어진다.**
`SD_BLE_EVENT_LENGTH`(현재 6 = 7.5 ms)를 함께 봐야 할 수 있고, 이건 실측 대상이다.

> ⚠ 설정을 바꿔 RAM 이 모자라면 `sd_ble_enable()` 이 **필요한 정확한 주소**를 돌려준다.
> `sdCfgResults()` 로 읽어서 링커 스크립트의 `RAM ORIGIN`/`LENGTH` 와
> `boards.txt` 의 `upload.maximum_data_size` **세 곳을 함께** 고쳐야 한다.

> **✅ 2026-09-05 실기 확인** (예약 `0x4780` 이던 시절):
> `.vectors @ 0x00000000`, `.text @ 0x00000E08`, `.data @ 0x20004780`,
> `__StackTop = 0x20040000`. 배치 규칙 자체는 그대로이고 시작 주소만 위로 옮겼다.
> 검증 기록: [HIL/M1-nu54dk.md](HIL/M1-nu54dk.md)

### 링커 스크립트 값

```
FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x158800
RAM   (rwx) : ORIGIN = 0x20007800, LENGTH = 0x38800
```

`boards.txt`:
```
upload.maximum_size      = 1411072   # 0x158800
upload.maximum_data_size =  231424   # 0x38800
```

---

## NU54-DK — nRF54L05 + S145 v10.0.1

출처: 같은 저장소의 `bm_nrf54l15dk_nrf54l05_cpuapp_s145_softdevice.dts`
칩 용량 근거: MDK 9.0.2 `nrf54l05_xxaa_application_memory.h`
(`NRF_MEMORY_FLASH_SIZE 0x0007D000`, `NRF_MEMORY_RAM_SIZE 0x00018000`)

### RRAM (500 KB = 0x7D000)

| 시작 | 크기 | 영역 |
|---|---|---|
| `0x00000000` | 354 KB (`0x58800`) | **application (slot0)** |
| `0x00058800` | 4 KB | peer_manager |
| `0x00059800` | 4 KB | storage0 |
| `0x0005A800` | 137 KB (`0x22400`) | **SoftDevice S145** |
| `0x0007D000` | — | RRAM 끝 |

### RAM (96 KB = 0x18000)

| 시작 | 크기 | 영역 |
|---|---|---|
| `0x20000000` | `0x4780` | **SoftDevice** |
| `0x20004780` | `0x13880` | **application** |
| `0x20018000` | — | RAM 끝 |

> DTS 의 `app_ram` 은 `DT_SIZE_K(78)` = `0x13800` 이라 상단 128 바이트가 남는다.
> 디바이스 트리가 크기를 K 단위로만 적기 때문에 생긴 내림이지 예약 영역이 아니다.
> 링커 스크립트는 `0x13880` 으로 RAM 끝까지 쓴다.

> **✅ 2026-09-06 실기 확인.** `__StackTop = 0x20018000`.
> Flash 36420 B (10% of 362496) / RAM 3856 B (4% of 80000).
> millis/micros 델타 정확. 검증 기록: [HIL/M1-tickless.md](HIL/M1-tickless.md)

### 링커 스크립트 값

```
FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x58800
RAM   (rwx) : ORIGIN = 0x20004780, LENGTH = 0x13880
```

`boards.txt`:
```
upload.maximum_size      = 362496   # 0x58800
upload.maximum_data_size =  80000   # 0x13880
```

### SoftDevice hex 는 SoC 별 재배치 빌드다

크기는 137 KB 로 같지만 로드 주소가 다르고 **서로 호환되지 않는다.**

```
s145_nrf54l05_10.0.1_softdevice.hex : :020000025000AC  -> 0x0005A800
s145_nrf54l15_10.0.1_softdevice.hex : :020000040015E5  -> 0x0015A800
```

`platform.txt` 의 `sd.hex` 는 `{build.sd_soc}` 로 파일을 고른다.
보드의 `menu.softdevice.*.build.sd_soc` 를 틀리면 SoftDevice 가 RRAM 밖에 써진다.

---

## 참고 — nRF54L15 + S115 v10.0.1 (미채택)

| 영역 | 값 |
|---|---|
| application | `0x00000000`, 1414 KB (`0x161800`) |
| storage | `0x00161800`, 8 KB |
| SoftDevice | `0x00163800`, 101 KB |
| SD RAM | `0x20000000`, `0x4380` |
| app RAM | `0x20004380`, `0x3BC80` |

S115는 peripheral 전용이라 Bluefruit의 Central 계열 API를 못 쓴다. 그래서 S145를 고정으로 택했다
(CLAUDE.md §8). 나중에 추가하려면 `boards.txt`에 메뉴 항목 하나와 링커 스크립트 하나만 더하면 된다.

---

## ⚠ nRF52와 정반대 배치

| | nRF52 + S140 | **nRF54L15 + S145** |
|---|---|---|
| `0x0` | SoftDevice | **application** |
| 상단 | application → bootloader | **SoftDevice** |
| 벡터 테이블 소유 | SoftDevice (MBR) | **application** |
| IRQ 전달 | SD가 앱으로 포워딩 | **앱이 SD로 포워딩** |

Adafruit 코어의 `nrf52840_s140_v6.ld`(`FLASH ORIGIN = 0x26000`)를 그대로 베끼면 안 된다.
앱이 `0x0`에서 시작하고 벡터 테이블도 앱 것이므로 VTOR 재배치가 필요 없다.
자세한 배경은 CLAUDE.md §7 F1.

---

## M4(부트로더) 레이아웃 — 제약은 이미 확정됐다

**결정된 것** (조사·실측 완료. CLAUDE.md §7 F11):

- **부트로더는 `0x0` 에 와야 한다.** nRF54L 에는 MBR 도 `UICR.BOOTLOADERADDR` 도 없어서
  (MDK 에 심볼 자체가 없다) CPU 가 `0x0` 에서 바로 부팅한다.
  nRF52 처럼 "MBR 이 상단의 부트로더를 찾아가는" 구조가 불가능하다
- **애플리케이션은 위로 밀린다.** nRF52 와 정반대다
- **SoftDevice 는 `0x0015A800` 고정.** hex 파일에 절대 주소로 박혀 있다
  (실측: `0x0015A800` ~ `0x0017C4F8`, 135.2 KB). 옮길 수 없다
- **본딩·스토리지는 이미 앱 파티션 밖이다** (`0x158800` ~ `0x15A800`).
  single-bank 업데이트로 날아가지 않는다. Nordic DTS 를 따른 결과다
- **VTOR 재배치는 코어가 이미 처리한다.** `cores/nrf54l/wiring.c` 의 `init()` 이
  링커 심볼 `__vectors_start` 에서 `SCB->VTOR` 을 설정하므로 앱 시작 주소가
  바뀌어도 자동으로 따라간다. MDK 스타트업은 VTOR 을 건드리지 않으므로 이게 없으면
  앱이 밀리는 순간 인터럽트가 부트로더 벡터로 간다

```
0x00000000  Bootloader              <- CPU 가 여기서 부팅 (크기 미정)
0x000?????  Application             <- 부트로더가 점프
0x00158800  peer_manager  4 KB      <- 앱 밖. 본딩 유지
0x00159800  storage0      4 KB
0x0015A800  SoftDevice  137 KB      <- 고정
0x0017C4F8  (끝)
```

**M4 에서 정할 것** (지금 정할 근거가 없다):

- 부트로더 크기 → 앱 시작 주소. 실제로 만들어 봐야 안다.
  `caveman99/nRF54_Bootloader` 를 먼저 빌드해 보면 현실적인 숫자가 나온다
- dual-bank 여부. **RRAM 은 erase 개념이 없어 swap 알고리즘 특성이 flash 와 다르다** (R9).
  실기 검증 전에는 확정하지 마라
- RRAM write block **16바이트** 정렬 (R9 / F5). 파티션 경계에 별도 정렬 요구가
  있는지는 미확인

바뀌는 것은 링커 스크립트의 `FLASH ORIGIN` 한 줄과 `boards.txt` 의
`upload.maximum_size` 한 줄뿐이다. 숫자를 미리 찍어두는 것보다 위 제약을 아는 게 중요하다.
