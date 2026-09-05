# M1 — tickless idle 실기 검증 (NU54-DK / nRF54L05)

`configUSE_TICKLESS_IDLE = 1` 을 켜기까지의 기록. 원인 규명에 가장 오래 걸린
구간이라 **어떻게 찾았는지**를 남긴다. 결론만 필요하면 CLAUDE.md §7 F9 / F9b 를 봐라.

## 최종 상태

| 항목 | 결과 |
|---|---|
| `configUSE_TICKLESS_IDLE` | **1** |
| millis 델타 vs 호스트 | 2000 ms vs 2000 ms |
| 틱 vs SYSCOUNTER 편차 | **0 ppm** (304 초 구간, 완전 일치) |
| 호스트 대비 클럭 오차 | **+38 ppm** (LFXO. tickless off 일 때 +25 ppm 과 같은 수준) |
| 5분 소크 | 샘플 151개, **이상 0건** (Δmillis≠2000 또는 Δmicros≠2000000 인 경우 0) |
| Flash / RAM | 38004 B (10%) / 3856 B (4%) |
| 보드 | NU54-DK, nRF54L05, LFXO |

## 증상

`configUSE_TICKLESS_IDLE` 을 1 로 바꾸는 순간:

- 틱이 **25~250 tick/s** 로 떨어진다 (목표 1000). 잴 때마다 값이 다르다
- `Serial` 이 완전히 죽는다 (`delay(1000)` 루프의 출력이 하나도 안 나옴)
- **폴트도 assert 도 없다.** `g_fault` / `g_assert_line` 전부 0
- 태스크는 죽지 않았다. `xTickCount` 는 느리게나마 계속 증가한다

## 왜 어려웠나 — 디버거가 증상을 지운다

SWD 로 들여다보면 **모든 레지스터가 정상으로 보인다**:

```
CC5      = SYSCOUNTER + 498 ms   (미래에 정확히 무장됨)
CCEN     = 1
INTEN2   = 0x20                  (채널 5 인터럽트 켜짐)
NVIC ISER[7] bit4 = 1            (GRTC_2_IRQn = 228 켜짐)
NVIC ISPR[7] bit4 = 0            (pending 없음)
IPR[228] = 0xE0                  (우선순위 7, 의도대로)
EVENTS_COMPARE[*] = 0
MODE     = 0x02                  (SYSCOUNTEREN=1, AUTOEN=0)
```

probe-rs 가 attach 하면서 코어를 halt/resume 하는 것이 **유일한 기상 요인**이었기
때문이다. 즉 관측 행위 자체가 증상을 일시적으로 없앤다.

## 어떻게 찾았나

### 1. 계측 변수로 시간 회계를 맞춰 본다

`vPortSuppressTicksAndSleep()` 안에 카운터를 심고 SWD 로 읽었다.

```
g_tl_inside_sum   WFI 구간 합계 (us)
g_tl_outside_sum  함수 밖에서 보낸 시간 합계 (us)
g_tl_woke_by_cc   기상 시점에 EVENTS_COMPARE 가 서 있던 횟수
g_tl_max_elapsed  가장 긴 슬립
g_tl_calls / g_tl_expected_sum / g_tl_completed_sum
```

8 초 구간 결과:

```
WFI 안  8,309,307 us  (103.8%)   ← 사실상 계속 자고 있다
함수 밖       132 us  (  0.0%)
max_elapsed  8,137,669 us        ← 한 번에 8.1 초를 잤다 (요청은 500 ms)
woke_by_cc  +1                   ← CC 로 깬 적이 거의 없다
calls       +4
```

디버거를 20 초 동안 떼고 두 번만 읽어 확인:

```
20 초 동안  슬립진입 +6,  CC기상 +1,  틱 +492
            (CC 가 정상이면 슬립진입 +40 근처여야 한다)
```

→ **CC 가 CPU 를 깨우지 못한다.**

### 2. 격리 시험 — 이게 결정적이었다

함수 본문을 이것만 남겼다. 틱 CC 는 건드리지 않고 1 ms 주기 그대로 둔다.

```c
void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    (void)xExpectedIdleTime;
    if (eTaskConfirmSleepModeStatus() == eAbortSleep) { return; }
    __DSB();
    __WFI();
}
```

결과: **정확히 1000 tick/s, Serial 정상, Δ 2000 ms.**

CC → NVIC → WFI 기상 경로 자체는 멀쩡하다는 뜻이다. 차이는 우리 함수의
**BASEPRI 크리티컬 섹션**뿐이었다.

### 3. 원인 — WFI 기상 조건은 BASEPRI 를 무시하지 않는다

ARMv7-M ARM B1.5.19 (ARMv8-M 동일):

> the assertion of an asynchronous exception that has sufficient priority to cause
> exception entry **when the value of PRIMASK is 0**. This means the value of PRIMASK
> does not affect whether an asynchronous exception is a WFI wake-up event, **but the
> values of FAULTMASK, BASEPRI, and the exception enables do affect this.**

틱 CC 는 커널 우선순위 7 이라 `BASEPRI = 0x20` 에 가려진다 → 영영 안 깬다.
FreeRTOS 표준 ARM_CM 포트가 이 함수에서만 `cpsid i`(PRIMASK)를 쓰는 이유다.

CLAUDE.md §7 F9 에 원래 정반대로("BASEPRI 로 마스크된 인터럽트도 WFI 를 깨운다")
적혀 있었다. 문서가 틀렸던 것이고, 지금은 고쳐져 있다.

### 4. 수정 — 마스크를 둘로 나눈다

SoftDevice 의 우선순위 0 zero-latency IRQ(RADIO_0/TIMER10/GRTC_3)를 막으면 안 되므로
PRIMASK 를 슬립 전체에 걸 수는 없다. 구간을 나눈다.

```c
__set_BASEPRI_MAX(SLEEP_BASEPRI);          /* 앱 IRQ 차단, SD prio-0 는 살려 둠 */
if (eTaskConfirmSleepModeStatus() == eAbortSleep) { 복구; return; }
/* ... CC 장전 ... */
__disable_irq();  __set_BASEPRI(0U);       /* 순서 중요: BASEPRI 를 내려야 깬다 */
__DSB(); __ISB();
__WFI();
__set_BASEPRI_MAX(SLEEP_BASEPRI);  __DSB(); __ISB();
__enable_irq();                            /* SD prio-0 즉시 실행 */
/* ... 틱 보정 ... */
__set_BASEPRI(prev_basepri);
```

SD 우선순위 0 이 지연되는 구간은 WFI 기상 직후 PRIMASK 를 푸는 몇 명령어뿐이다.

## 두 번째 문제 — +253 ppm 드리프트

위 수정 후 동작은 하는데 `micros` 델타가 2000507 us / `millis` 델타가 2000 ms 였다.
틱이 **+253 ppm 느리다.** LFXO 로 잡아 둔 +25 ppm 을 통째로 날리는 크기다.

원인: 기상 시각 기준으로 다음 CC 를 잡으면 매 슬립마다
`elapsed % CYCLES_PER_TICK` (최대 999 us)이 버려진다.

수정: 1 ms **틱 그리드**를 유지한다.

- `m_last_cc` 를 "다음 틱의 절대 시각"으로 두고 항상 그리드 위에 유지
- `xExpectedIdleTime` 틱을 자려면 CC 를 `grid_next + (n-1) * CYCLES_PER_TICK` 에 건다
  (마지막 1 틱은 `vTaskStepTick()` 이 센다)
- 기상 후 `completed = 1 + (exit - grid_next) / CYCLES_PER_TICK`,
  다음 CC 는 `grid_next + completed * CYCLES_PER_TICK`

결과: **0 ppm.**

`millis()` 와 `micros()` 를 각각 보면 둘 다 정상으로 보인다.
**둘의 차이를 봐야 드러나는** 종류의 버그라, 회귀 시험은 반드시 차이를 본다.

## 아직 안 한 것

- **전류 측정.** F8 에 따라 SWD 프로브를 물리적으로 분리하고 재야 한다.
  tickless 를 켠 목적이 전력이므로 이게 빠지면 검증이 끝난 게 아니다
- `nrfx_grtc_active_request_set(true)` 를 빼도 되는지.
  MODE.AUTOEN 이 이미 0 이라 중복일 가능성이 높다. 전력 측정 때 같이 판단한다
- BLE 와의 공존. BASEPRI/PRIMASK 분리가 실제로 라디오 타이밍을 지키는지는
  M3 에서 advertising 유지 + tickless 동시 동작으로 확인해야 한다
