# 메모리 맵 — NU54-DK (nRF54L15)

출처: `nrfconnect/sdk-nrf-bm` v2.0.1
`boards/nordic/bm_nrf54l15dk/bm_nrf54l15dk_nrf54l15_cpuapp_s145_softdevice.dts`
(및 같은 디렉토리의 `..._s115_softdevice.dts`)

MCUboot 없음 / TrustZone 없음(secure-only, R5) 기준.

---

## nRF54L15 + S145 v10.0.1 — **채택 구성**

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
| `0x20000000` | `0x4780` (18,304 B) | **SoftDevice** |
| `0x20004780` | `0x3B880` (~237 KB) | **application** |

> **✅ 2026-09-05 실기 확인.** 실제 링크 결과가 아래와 일치한다:
> `.vectors @ 0x00000000`, `.text @ 0x00000E08`, `.data @ 0x20004780`,
> `.bss @ 0x20004A80`, `__StackTop = 0x20040000`, `__StackLimit = 0x2003F800`.
> 힙 가용 232 KB (`__HeapBase 0x200055C4` ~ `__StackLimit`).
> 검증 기록: [HIL/M1-nu54dk.md](HIL/M1-nu54dk.md)

### 링커 스크립트 값

```
FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x158800
RAM   (rwx) : ORIGIN = 0x20004780, LENGTH = 0x3B880
```

`boards.txt`:
```
upload.maximum_size      = 1411072   # 0x158800
upload.maximum_data_size =  243328   # 0x3B880
```

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
