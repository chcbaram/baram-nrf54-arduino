# M3 A단계 실기 검증 — SoftDevice 기동과 advertising

날짜: 2026-09-06
보드: Seeed XIAO nRF54L15 Sense (nRF54L15)
SoftDevice: S145 v10.0.1 (`0x0015A800`, 137 KB)
호스트: macOS / probe-rs 0.32.0 / bleak (BLE 스캔·연결)

**이 단계의 목적은 advertising 자체가 아니라 §7 F9 의 답을 얻는 것이었다** —
tickless idle 의 BASEPRI/PRIMASK 분리가 라디오와 실제로 공존하는가.
CLAUDE.md 가 "M3 전까지 검증할 수 없다"고 적어 둔 이 프로젝트 최대의 미지수다.

---

## 1. 결과 — F9 는 통과다

| 항목 | 결과 |
|---|---|
| `sd_softdevice_enable()` | ✅ SD RAM **18304 B** (= `0x4780`, 링커 예약치와 정확히 일치) |
| `sd_ble_enable()` | ✅ |
| advertising 시작 | ✅ `err = 0` |
| **공중 확인** | ✅ `BARAM-nRF54L`, RSSI −44, 8초에 21회 (설정 100 ms 간격과 일치) |
| **60초 연속 advertising** | ✅ 78회 탐지, 끊김 없음 |
| **실제 연결 성립** | ✅ 장치에 `conn=1 disc=1` 기록 |
| **틱 vs SYSCOUNTER** | ✅ **0.0 ppm** (138초, 70샘플) |
| Δmillis | ✅ 전부 정확히 2000 |

**advertising 과 연결이 도는 동안에도 틱이 어긋나지 않았다.** F9 의 BASEPRI/PRIMASK
분리 구조를 바꿀 이유가 없다.

> 호스트(bleak)는 연결 후 `TimeoutError` 를 냈다. **정상이다.** A 단계에는 GATT
> 서비스가 하나도 없어서 서비스 탐색이 빈 손으로 끝난다. 링크 계층 연결 자체는
> 장치 쪽 카운터로 확인됐다.

### 관측된 것 두 가지

**(a) Δmicros 지터 ±29 µs** — SD 를 켠 상태에서 69샘플 중 6개.
SD 없이 같은 시험을 하면 ±1 µs 다. 라디오의 zero-latency IRQ(우선순위 0)가
`loop()` 태스크를 선점해 출력 시점이 흔들리는 것이고, **누적되지 않는다**
(Δmillis 는 항상 정확히 2000, 틱 vs SYSCOUNTER 0.0 ppm).

**(b) 호스트 대비 클럭이 −19 ppm → +42 ppm 으로 이동** — SD 를 켰을 때만.
원인 미확정. 확인한 것과 남은 것:

- LFXO 내부 로드 캡은 **SD 가 건드리지 않았다.** 실측 `XOSC32KI.INTCAP = 0x15`(21)
  로, 우리가 FICR 트림에서 계산해 넣은 값 그대로다 (`0x50120904`)
- GRTC `AUTOEN` 을 0→1 로 바꾼 것 때문도 **아니다.** SD 없이 AUTOEN=1 로 재측정해
  −19 ppm 이 나왔다 (AUTOEN=0 일 때 −14 ppm, 측정 오차 범위)
- 남은 후보: SD 자체의 LFCLK 관리, 또는 라디오 부하에서 USB CDC 도착 시각이
  체계적으로 밀리는 **측정 방식의 한계**. 지금 방법(시리얼 도착 시각)으로는
  둘을 못 가른다. GPIO 토글 + 스코프 같은 독립 기준이 필요하다
- **BLE 요구치 ±250 ppm 안이므로 M3 진행을 막지 않는다.** 다만 숫자를 기록해 둔다

---

## 2. 막혔던 곳 — 오류 코드 네 개를 순서대로 밟았다

전부 "sd_softdevice_enable() 이 그냥 실패" 로 보이지만 원인은 매번 달랐다.
**헤더의 retval 주석이 정답을 갖고 있었다.**

### (1) `NRF_ERROR_INVALID_PARAM` (0x07)

`nrf_clock_lf_cfg_t.hfint_ctiv = 0` 으로 뒀다. `nrf_sdm.h` 는 이 필드를
**1~255** 로 규정한다. "LFCLK 이 크리스털이니 HFINT 보정은 무관하다" 고
생각하기 쉬운데, 소스와 무관하게 유효한 값이어야 한다.
→ sdk-nrf-bm Kconfig 기본값인 **60**.

### (2) `NRF_ERROR_SDM_INCORRECT_GRTC_CONFIGURATION` (0x1003) ⭐

`nrf_sdm.h` 원문이 조건을 그대로 적어 놨다:

> "GRTC is not running with SYSCOUNTER on **or AUTOEN is not set**"

우리는 `NRFX_GRTC_CONFIG_AUTOEN = 0` 이었다. M1 에서 "AUTOEN 을 끄고 명시적
active request 를 건다" 고 정해 둔 것인데, **SoftDevice 는 AUTOEN 을 요구한다.**
→ `NRFX_GRTC_CONFIG_AUTOEN = 1`. Zephyr 의 `nrf_grtc_timer.c` 도 같은 구성이다
(`nrfx_grtc_active_request_set(true)` 를 쓴다).

M1 회귀 없음: SD 없이 −19 ppm, 틱 0.0 ppm, Δmicros 지터 ±1 µs.

**곁가지로 확인한 것**: GRTC `CLKSEL` 을 LFXO 로 직접 박아 둔 것도 같이 고쳤다
(→ SystemLFCLK). 이건 이 오류의 원인은 아니었지만 — 바꿔도 0x1003 이 그대로였다 —
SoftDevice 가 LFCLK 를 관리한다는 전제와 맞고 Zephyr 기본값과도 같다.
정확도 손해는 없다. `lfclk_start()` 가 이미 시스템 LFCLK 를 LFXO 로 맞춰 둔다.

### (3) `NRF_ERROR_INVALID_STATE` (0x08) — `sd_ble_enable()` 에서 ⭐

`ble.h` 원문:

> "The BLE stack had already been initialized and cannot be reinitialized,
> **or the random number generator has not been seeded.** See sd_rand_seed_set."

앞쪽만 읽으면 "이미 초기화됨" 으로 오해한다. 실제 원인은 **뒤쪽**이었다.
SoftDevice 는 켜진 직후 `NRF_EVT_RAND_SEED_REQUEST` SoC 이벤트를 올리고,
시드를 받기 전에는 `sd_ble_enable()` 을 거부한다.

→ `sd_evt_get()` 으로 SoC 이벤트를 꺼내 **CRACEN TRNG**
(`nrfx_cracen_entropy_get()`) 로 32바이트를 만들어 `sd_rand_seed_set()` 에 넘긴다.
nRF52 의 RNG 페리페럴이 아니다 — `nrfx/VENDORING.md` 에서 `nrfx_rng.c` 를 뺀 이유다.

**그리고 `sd_ble_enable()` 전에 동기적으로 폴링해야 한다.** 펌프 태스크가
처리하기를 기다리면 순서가 어긋난다. 원본 `nrf_sdh.c` 도 같은 이유로
`sd_ble_enable()` 앞에서 직접 폴링한다.

### (4) 아무 출력도 없던 구간 — 오진이었다

처음에 시리얼이 완전히 조용해서 "부팅부터 죽었다" 고 판단했는데 아니었다.
스케치가 오류를 **한 번만** 찍고 무한 대기로 들어갔고, 그 한 줄이 나올 때
포트를 아직 열지 않았을 뿐이다.

→ 실패 경로에서도 계속 찍게 고쳤다. **부팅 직후 한 번만 찍는 진단은 USB CDC
장치에서 놓치기 쉽다.**

---

## 3. 어떻게 찾았나 — 단계 마커

`sd_softdevice_enable()` 이 실패한 건지 그 앞에서 멈춘 건지 구분이 안 됐다.
probe-rs 는 halt 를 유지하지 못하므로(docs/HIL/M1-nu54dk.md §5) `g_sd_stage` 에
진행 단계를 남기고 SWD 로 읽었다.

```
g_sd_stage = 7  ->  sd_softdevice_enable() 은 반환했다. 그 뒤가 문제다
g_sd_stage = 10 ->  sd_ble_enable() 까지 갔다. 0x08 은 여기서 나온 것이다
```

`m_last_error` / `g_sd_already_enabled` 를 같이 읽어 **어느 호출이** 실패했는지를
좁혔다. 이 마커들은 코드에 남겨 뒀다.

---

## 3.5 B1 — 커스텀 GATT 서비스 (2026-09-06)

`nrf54l/libraries/Bluefruit52Lib/` 에 `BLEUuid` / `BLEService` /
`BLECharacteristic` + 최소 `Bluefruit` 싱글턴을 올렸다. 호스트(bleak)에서 확인:

| 항목 | 결과 |
|---|---|
| 서비스 탐색 | ✅ 128비트 UUID 그대로 보인다 |
| 특성 속성 | ✅ `notify/read`, `write/write-without-response` |
| 읽기 | ✅ |
| 알림(notify) | ✅ CCCD 구독 후 수신 |
| 쓰기 → 콜백 | ✅ 장치에서 `rx=1 last=ping-42` |
| 연결 해제 후 자동 재광고 | ✅ |

### 막힌 것 — 연결은 되는데 ATT 가 전혀 안 흐른다 ⭐

`BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST` (0x23) 에 답하지 않았다.
그러면 **연결은 20초 넘게 멀쩡히 유지되는데** 링크 계층 절차가 끝나지 않아
호스트의 서비스 탐색이 시작조차 못 하고, 결국 호스트가 끊는다 (reason 0x13).

찾은 방법은 단순했다 — **관찰자를 하나 더 달아 이벤트 ID 를 그대로 찍었다.**

```
[BLE] connected 3
  evt 0x10   BLE_GAP_EVT_CONNECTED
  evt 0x21   BLE_GAP_EVT_PHY_UPDATE_REQUEST      <- 우리가 답했다
  evt 0x22   BLE_GAP_EVT_PHY_UPDATE              <- 그래서 왔다
  evt 0x23   BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST  <- 답하지 않았다
  (GATTS 이벤트가 하나도 오지 않는다)
[BLE] disconnected 3 reason=0x13
```

→ `sd_ble_gap_data_length_update(conn, NULL, NULL)`. NULL 이면 SoftDevice 가
최대치를 고른다. 함께 `CONN_PARAM_UPDATE_REQUEST` 도 상대 제안대로 수락한다.

> **연결이 성립했는데 아무 데이터도 안 흐르면 GAP 이벤트부터 찍어라.**
> "GATT 구현이 잘못됐나" 를 먼저 의심하기 쉬운데, 실제로는 그 앞 단계에서
> 멈춰 있었다.

또 하나: notify 를 쓰는 특성은 **CCCD 의 쓰기 권한을 열어야 한다.**
상대가 CCCD 에 써야 알림이 켜지기 때문이다. 빠뜨리면 증상이
"연결은 되는데 데이터가 안 온다" 로 똑같이 보인다.

---

## 3.6 B2 — BLEUart (NUS) (2026-09-06)

`BLEUart` 를 `BLEService` + `Stream` 상속으로 올렸다. `Serial` 처럼 쓴다.

| 항목 | 결과 |
|---|---|
| 서비스 탐색 (NUS UUID) | ✅ |
| 장치 → 호스트 (notify) | ✅ `uptime N` 주기 송신 |
| 호스트 → 장치 (write) | ✅ 바이트 전부 도달 |
| 에코 왕복 (18바이트) | ✅ 완전 일치 |
| **MTU(20바이트) 초과 쓰기** | ❌ 연결이 끊긴다 — 아래 참조 |

### 잡은 것 — notify 를 연속으로 보내면 조용히 사라진다 ⭐

10바이트를 에코했는데 **3바이트만** 호스트에 도착했다. 장치 쪽 로그에는
10바이트가 전부 찍혀 있었으므로 수신은 정상이었고, 송신에서 사라진 것이다.

원인: `sd_ble_gatts_hvx()` 의 **notify 큐가 얕다 (기본 1건).** 연속 호출하면
곧 `NRF_ERROR_RESOURCES` 가 나는데, 그때 그냥 실패로 처리하고 버리고 있었다.

→ `BLE_GATTS_EVT_HVN_TX_COMPLETE` 를 세마포어로 기다렸다가 재시도한다
(`BLE_HVX_MAX_RETRY` 8회 / 회당 200 ms). 수정 후 18바이트 왕복이 완전히 일치했다.

> **`sd_ble_gatts_hvx()` 의 반환값을 "성공/실패" 로만 보면 안 된다.**
> `NRF_ERROR_RESOURCES` 는 "지금은 자리가 없다" 이지 오류가 아니다.
> 이걸 오류로 다루면 데이터가 소리 없이 없어진다.

### 아직 안 되는 것 — MTU 초과 쓰기

호스트가 20바이트(= 기본 ATT MTU 23 − 헤더 3)를 넘겨 쓰면 **연결이 끊긴다.**
ATT long write(prepare/execute) 경로를 다루지 않기 때문이다.

정공법은 **MTU 협상**이다. 지금은 `BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST` 에
기본값 23 으로 답하고 있다. `sd_ble_enable()` **전에**
`sd_ble_cfg_set(BLE_CONN_CFG_GATT, ...)` 로 `att_mtu` 를 키우면 한 패킷에
훨씬 많이 실을 수 있어 long write 자체가 필요 없어진다.
Adafruit 의 `Bluefruit.configPrphBandwidth()` 가 하는 일이 이것이다. → B3.

---

## 3.7 B3 — MTU 협상 (2026-09-06)

기본 ATT MTU(23)로는 한 번에 20바이트밖에 못 싣고, 그보다 큰 쓰기는 ATT
long write 가 필요해 **연결이 끊겼다.** `sd_ble_cfg_set()` 으로 MTU 를 키워
해결했다.

| 항목 | 결과 |
|---|---|
| 협상된 MTU | ✅ **247** (호스트 확인) |
| 75 바이트 쓰기 → 에코 | ✅ 완전 일치 |
| 200 바이트 쓰기 → 에코 | ✅ 완전 일치 |
| SoftDevice RAM 요구 | `0x20003750` (링커 `0x20004780` 아래 → **링커 변경 불필요**) |

### 막힌 것 네 개 — 전부 헤더 주석에 답이 있었다

**(1) `conn_cfg_tag` 에 0 을 쓰면 안 된다.** `ble.h` 원문:

> "Must be different for all connection configurations added and
> **not `BLE_CONN_CFG_TAG_DEFAULT`**"

0 을 넣으면 `sd_ble_cfg_set()` 이 **성공을 돌려주는데도 설정이 안 먹는다.**
증상은 "MTU 를 247 로 구성했는데 협상 결과가 계속 23" 이다.

**(2) `sd_ble_gap_adv_start()` 에도 같은 태그를 넘겨야 한다.** 한쪽만 바꾸면
설정은 등록됐는데 연결은 기본 구성으로 만들어진다. 태그만 1 로 바꾸고
`adv_start` 를 그대로 뒀더니 **advertising 자체가 실패**했다.

**(3) `adv_set_count` 를 빠뜨리면 안 된다** ⭐ — 가장 찾기 어려웠다.

`ble_gap_cfg_role_count_t` 의 **첫 필드**가 `adv_set_count` 다. `memset` 으로
0 을 만들고 `periph_role_count` 만 채우기 쉬운데, `ble_gap.h` 가 이 조합을
명시적으로 거부한다:

> "`NRF_ERROR_INVALID_PARAM` — **adv_set_count is 0 and periph_role_count is
> non-zero.**"

그러면 역할 수가 기본값(peripheral 1 / central 3)으로 남고, 이어지는
`sd_ble_enable()` 이 `conn_count` 와 안 맞아 **`NRF_ERROR_NOT_SUPPORTED`(0x06)**
로 실패한다. 겉으로는 "BLE 를 못 켠다" 로만 보인다.

→ `adv_set_count = BLE_GAP_ADV_SET_COUNT_DEFAULT`.

**(4) MTU 교환 응답에 기본값을 하드코딩하면 안 된다.** `bluefruit.cpp` 가
`BLE_GATT_ATT_MTU_DEFAULT` 로 답하고 있었다. 스택을 크게 구성해도 상대와는
23 으로 협상된다. 실효 MTU 는 양쪽 제시값 중 작은 쪽이다.

### 그리고 하나 더 — 수신 FIFO 가 MTU 를 못 따라간다

MTU 247 로 협상된 뒤 75바이트를 보냈더니 **62바이트만 에코**됐다.
`BLEUart` 가 코어의 `RingBuffer`(`SERIAL_BUFFER_SIZE` = 64)를 쓰고 있었고,
넘치는 만큼 조용히 사라진 것이다.

→ 자체 링버퍼로 바꾸고 `BLE_UART_RX_FIFO_SIZE`(기본 256)로 뺐다.
버려진 바이트는 `bleuart.dropped()` 로 보인다 — **조용히 사라지지 않게 했다.**

> **MTU 를 키우면 그 뒤의 버퍼도 같이 봐야 한다.** 스택만 키우고 애플리케이션
> 버퍼를 그대로 두면 손실이 애플리케이션 쪽으로 옮겨갈 뿐이다.

### 진단 방법

`sd_ble_cfg_set()` 세 건의 반환값을 **시리얼로 직접 찍는 것**이 결정적이었다.
전역에 담고 SWD 로 읽으려 했으나 `--gc-sections` 에 지워져 안 보였다.
`sdCfgResults()` 로 남겨 뒀다.

```
begin=1  err=0x00000000
cfg role=0x00000000 gap=0x00000000 gatt=0x00000000  MTU=247 tag=1
RAM: 링커=0x20004780  SD요구=0x20003750
```

---

## 4. 남은 것

- **GATT 서비스가 없다.** 연결은 되지만 서비스 탐색이 빈 손이라 호스트가 곧 끊는다.
  B 단계(`BLEService` / `BLECharacteristic`)에서 채운다
- **SoC 이벤트를 시드 요청 말고는 무시한다.** 플래시 동작 완료, 전원 경고,
  라디오 타임슬롯은 필요해질 때 붙인다
- **HardFault 를 SoftDevice 로 포워딩하지 않는다.** 개발 중 `g_fault` 기록기를
  살리려는 의도적 결정이다 (`sd_irq_forward.S` 주석). SoftDevice 내부 폴트 처리가
  필요해지면 되돌린다
- **`sd_ble_cfg_set()` 을 아직 부르지 않는다.** 기본 구성(peripheral 1 링크)으로
  돌고 있다. central 이나 MTU 확장이 필요하면 B 단계에서 넣는다
- **전류 미측정.** SD 를 켜면 바닥 전류가 올라간다. XIAO 는 RF 스위치 전원 몫도 섞인다
- **클럭 이동(+42 ppm) 원인 미확정.** §1 (b)

---

## 5. 재현

```sh
# SoftDevice 를 먼저 굽는다 (앱과 별개 영역이라 지워지지 않는다)
probe-rs download --chip nRF54L15 --binary-format hex --verify \
    nrf54l/softdevice/s145_nrf54l15_10.0.1_softdevice.hex

arduino-cli compile --fqbn baram-nrf54:nrf54l:xiao_nrf54l15 --build-path /tmp/b <스케치>
arduino-cli upload  -b baram-nrf54:nrf54l:xiao_nrf54l15 --input-dir /tmp/b <스케치>

# 포워딩 벡터가 실렸는지 — 전부 T 여야 한다
arm-none-eabi-nm /tmp/b/*.elf | grep -E "T (RADIO_0|TIMER10|GRTC_3|SWI00)_IRQHandler"

# 공중 확인 (macOS, bleak). Bluetooth 를 켜 두어야 한다
python3 -c "
import asyncio
from bleak import BleakScanner
async def m():
    d = await BleakScanner.find_device_by_name('BARAM-nRF54L', timeout=10)
    print(d)
asyncio.run(m())"
```

⚠ macOS 에서 `BleakScanner` 가 `Bluetooth device is turned off` 를 내면 실제로
꺼져 있거나 **상태 전이를 기다리지 않은 것**이다. CoreBluetooth 는 런루프를
돌려야 `CBManagerState` 가 갱신된다.
