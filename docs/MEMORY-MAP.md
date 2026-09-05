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

## M4(부트로더) 진입 시 재검토할 것

부트로더를 넣으면 이 맵이 바뀐다. 확정 전에 다음을 정해야 한다:

- 부트로더 위치 — 앱이 `0x0`을 쓰고 있으므로 부트로더를 `0x0`에 두고 앱을 밀지, 상단에 둘지
- nRF54L의 부트로더 진입 주소 메커니즘 (nRF52 `UICR.NRFFW[0]` 상당물) 존재 여부
- RRAM write block **16바이트** 정렬 (R9 / F5)
- single-bank 또는 overwrite-only. swap 알고리즘 금지
- `.noinit` 리텐션 영역 — `systemOff()`의 RAM 리텐션 해제와 충돌하지 않게 (CLAUDE.md §6.1)
