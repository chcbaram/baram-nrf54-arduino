#!/usr/bin/env python3
"""
핀맵 예제 생성기 — baram-nrf54l-arduino
SPDX-License-Identifier: MIT

Nordic Pin Planner 의 SoC 정의에서 "이 핀에 무엇을 붙일 수 있나" 를 뽑아
`libraries/PinMap/examples/` 아래의 예제 주석으로 굽는다.

    https://github.com/NordicPlayground/PinPlanner

손으로 쓰지 않는 이유는 하나다 — 표가 156 줄짜리 제약이라 사람이 옮겨 적으면
반드시 틀리고, Nordic 이 JSON 을 고쳤을 때 따라가지 못한다.

보드 레벨 표는 `docs/boards/<보드>.md` 의 확장 헤더 블록을 그대로 읽는다.
핀 배정의 정본이 그 문서라고 CLAUDE.md §4.1 이 정해 두었으므로, 여기서
다시 적지 않는다.

사용:
    python3 extras/gen_pinmap.py            # 전부 생성
    python3 extras/gen_pinmap.py --print nu54dk   # 미리보기
"""
import json, os, re, sys, urllib.request
from collections import OrderedDict

ROOT  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, '.pinplanner-cache')
RAW   = "https://raw.githubusercontent.com/NordicPlayground/PinPlanner/main/mcus"


def fetch(soc, package):
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, f"{soc}_{package}.json")
    if not os.path.exists(path):
        url = f"{RAW}/{soc}/{package}.json"
        sys.stderr.write(f"  받는 중 {url}\n")
        urllib.request.urlretrieve(url, path)
    return json.load(open(path))


def pin_table(doc):
    """{'P1.08': ['TWIM20.SCL', 'PWM20.OUT[0]', ...]} 를 만든다.

    allowedGpio 는 두 형태로 적힌다 — 'P1*' 는 그 포트 전체, 'P2.01' 은 그 핀만.
    와일드카드를 풀어 두어야 핀 기준으로 뒤집을 수 있다.
    """
    pins = [p['name'] for p in doc['pins'] if re.fullmatch(r'P\d+\.\d+', str(p.get('name', '')))]
    table = OrderedDict((p, []) for p in sorted(pins, key=_pin_key))
    wildcard = {}

    for per in doc['socPeripherals']:
        pid = per['id']
        for sig in per.get('signals', []):
            label = f"{pid}.{sig['name']}"
            for allowed in sig.get('allowedGpio', []):
                if allowed.endswith('*'):
                    wildcard.setdefault(allowed[:-1], []).append(label)
                elif allowed in table:
                    table[allowed].append(label)
    return table, wildcard


def _pin_key(name):
    port, idx = name[1:].split('.')
    return (int(port), int(idx))


def board_headers(md_path):
    """`docs/boards/*.md` 에서 확장 헤더를 읽는다.

    보드 문서가 핀 배정의 정본이므로(CLAUDE.md §4.1) 여기서 다시 적지 않고 읽는다.
    다만 문서마다 적는 방식이 달라 두 형태를 모두 받는다:

      ① 코드블록      NU54-DK  — '**P1 헤더**' 다음 ``` 블록에 '1 P0.00' 나열
      ② 마크다운 표   XIAO     — '| 핀 | 이름 | GPIO |' 표. 좌우 두 벌이 한 줄에 있다

    반환: [(헤더이름, [(핀번호, 'P0.00', '별명' 또는 ''), ...]), ...]
    """
    text = open(md_path, encoding='utf-8').read()
    out = []

    for m in re.finditer(r'\*\*(\S+)\s*헤더\*\*\s*\n```\n(.*?)\n```', text, re.S):
        entries = [(int(n), sig, '') for n, sig in re.findall(r'(\d+)\s+(\S+)', m.group(2))]
        out.append((m.group(1), sorted(entries)))

    for m in re.finditer(r'###\s*(.+?헤더.*?)\n\n((?:\|.*\n)+)', text):
        rows = []
        for line in m.group(2).strip().split('\n'):
            cells = [c.strip() for c in line.strip('|').split('|')]
            # 한 줄에 (핀, 이름, GPIO) 가 여러 벌 들어 있다. 3칸씩 훑는다.
            for i in range(0, len(cells) - 2):
                num, name, gpio = cells[i], cells[i + 1], cells[i + 2]
                if num.isdigit() and re.fullmatch(r'P\d+\.\d+', gpio):
                    rows.append((int(num), gpio, name))
        if rows:
            out.append((m.group(1).strip(), sorted(set(rows))))

    return out


def variant_usage(variant_h):
    """이 핀을 **스케치에서 무슨 이름으로 부르는가.**

    `PIN_LED1` 만 보여 주면 부족하다. 사람이 실제로 치는 것은 `LED_BUILTIN`
    이고, 그게 어느 핀인지 모르면 "왜 analogWrite 가 안 되지" 로 끝난다.
    그래서 별칭을 끝까지 따라간다:

        #define PIN_LED1    _PINNUM(2, 9)      <- 뿌리
        #define LED_BUILTIN PIN_LED1           <- 별칭
        static const uint8_t A0 = PIN_A0;      <- Arduino 관례의 별칭

    반환: 'P2.09' -> ['PIN_LED1', 'LED_BUILTIN', 'LED_RED']
    """
    text = open(variant_h, encoding='utf-8').read()

    # ⚠ 주석을 먼저 걷어낸다. variant.h 는 "바로잡히면 이 네 줄을 추가하면 된다"
    #   식으로 **주석 안에 #define 예시**를 적어 두는데, 그걸 읽으면 아직 없는
    #   핀이 표에 나타난다. 실제로 PIN_SERIAL1_TX 가 그렇게 새어 나왔다.
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)

    root = {}      # 매크로 이름 -> 'P2.09'
    for m in re.finditer(r'#define\s+(\w+)\s+_PINNUM\(\s*(\d+)\s*,\s*(\d+)\s*\)', text):
        root[m.group(1)] = f"P{int(m.group(2))}.{int(m.group(3)):02d}"

    # 별칭 — 값이 이미 아는 이름인 것. 사슬이 길 수 있어 안정될 때까지 돈다.
    alias = []
    alias += re.findall(r'#define\s+(\w+)\s+(\w+)\s*(?:/\*|//|$)', text, re.M)
    alias += re.findall(r'static\s+const\s+\w+\s+(\w+)\s*=\s*(\w+)\s*;', text)
    alias += [(n, f"__P{p}_{i}") for n, p, i in
              re.findall(r'static\s+const\s+\w+\s+(\w+)\s*=\s*_PINNUM\(\s*(\d+)\s*,\s*(\d+)\s*\)', text)]
    for n, p, i in re.findall(r'static\s+const\s+\w+\s+(\w+)\s*=\s*_PINNUM\(\s*(\d+)\s*,\s*(\d+)\s*\)', text):
        root[n] = f"P{int(p)}.{int(i):02d}"

    order = list(root)
    for _ in range(4):
        for name, target in alias:
            if name not in root and target in root:
                root[name] = root[target]
                order.append(name)

    used = OrderedDict()
    for name in order:
        used.setdefault(root[name], [])
        if name not in used[root[name]]:
            used[root[name]].append(name)
    return used


def shorten(labels):
    """'SPIM/SPIS20.SCK' 처럼 긴 이름을 줄이고 같은 페리페럴끼리 묶는다."""
    groups = OrderedDict()
    for lab in labels:
        per, _, sig = lab.rpartition('.')
        groups.setdefault(per, []).append(sig)
    return ', '.join(f"{p}.{'/'.join(s)}" if len(s) > 1 else f"{p}.{s[0]}"
                     for p, s in groups.items())


def port_common(wildcard):
    """포트 전체가 쓸 수 있는 것. 페리페럴 이름만 모은다 — 그 포트면 신호가
    다 되므로 신호까지 적으면 길이만 늘고 정보가 없다."""
    out = OrderedDict()
    for port, labels in sorted(wildcard.items()):
        pers = []
        for lab in labels:
            per = lab.rpartition('.')[0]
            if per not in pers:
                pers.append(per)
        out[port] = pers
    return out


def wrap(items, width=64, indent=' ' * 11):
    """쉼표 목록을 폭에 맞춰 접는다."""
    lines, cur = [], ''
    for it in items:
        piece = (cur + ', ' + it) if cur else it
        if len(piece) > width and cur:
            lines.append(cur)
            cur = it
        else:
            cur = piece
    if cur:
        lines.append(cur)
    return ('\n' + indent).join(lines)


def arduino_caps(pin, table, wildcard):
    """이 핀에서 Arduino 함수가 되는가.

    표가 "P1 에 PWM 이 있다" 만 보여 주면, **이 핀은 PWM 이 안 된다** 는 사실은
    없는 것을 눈치채야 알 수 있다. 부정형이 더 중요한 정보라 따로 낸다.

    반환: (PWM 여부, 인터럽트 여부, 'AIN3' 또는 '')
    """
    labels = list(table.get(pin, [])) + wildcard.get(pin.split('.')[0], [])
    pwm = any(l.startswith('PWM') for l in labels)
    irq = any(l.startswith('GPIOTE') for l in labels)
    ain = next((l.rpartition('.')[2] for l in labels
                if l.startswith('SAADC.AIN')), '')
    return pwm, irq, ain


def caps_summary(table, wildcard):
    """Arduino 함수별로 '어느 핀이 되는가' 를 한 덩어리로 낸다.

    핀 기준 표는 "이 핀에 뭐가 되나" 에 답하지만, 사람은 대개 반대로 묻는다 —
    "analogWrite 를 어디에 걸 수 있나". 그쪽도 답해 준다.
    """
    pwm, irq, adc = [], [], []
    for pin in table:
        p, i, a = arduino_caps(pin, table, wildcard)
        if p: pwm.append(pin)
        if i: irq.append(pin)
        if a: adc.append(f"{pin}({a})")
    return pwm, irq, adc


def fold_ports(pins):
    """['P1.00', ..., 'P1.16'] -> 'P1 전체' 처럼 줄인다."""
    from collections import defaultdict
    byport = defaultdict(list)
    for p in pins:
        byport[p.split('.')[0]].append(p)
    out = []
    for port, lst in sorted(byport.items()):
        out.append(f"{port} 전체 ({len(lst)}핀)")
    return ', '.join(out) if out else '없다'


def render_caps(table, wildcard):
    pwm, irq, adc = caps_summary(table, wildcard)
    all_ports = sorted({p.split('.')[0] for p in table})
    L = [" Arduino 함수 — 되는 곳", ""]
    L.append(f"   analogWrite      PWM20/21/22    {fold_ports(pwm)}")
    L.append(f"   attachInterrupt  GPIOTE20/30    {fold_ports(irq)}")
    L.append(f"   analogRead       SAADC AIN0~7   {wrap(adc, 58, ' ' * 34) if adc else '없다'}")
    L.append("")
    dead = [p for p in all_ports
            if not any(x.startswith(p + '.') for x in pwm + irq + [a.split('(')[0] for a in adc])]
    for port in dead:
        L.append(f"   ⚠ {port} 에는 셋 다 없다 — 하드웨어가 없는 것이라 코어가 해 줄 수 있는 일이 아니다")
    if dead:
        L.append("")
    return L


CAPS_LEGEND = [
    " PWM = analogWrite   IRQ = attachInterrupt   ADC = analogRead",
    " 'x' 는 **그 핀에서 그 함수를 쓸 수 없다** 는 뜻이다. 하드웨어가 없는 것이라",
    " 코어가 나중에 지원해 주는 종류의 것이 아니다.",
]


def render_board(board, chip, soc, package, md_path, variant_h):
    doc = fetch(soc, package)
    table, wildcard = pin_table(doc)
    used = variant_usage(variant_h)
    common = port_common(wildcard)
    L = []

    L.append(f" {board} — {chip}, {package}")
    L.append("")
    L.append(" 포트 공통 — 그 포트의 어느 핀에나 붙는다")
    L.append("")
    for port, pers in common.items():
        L.append(f"   {port + '*':6s}  {wrap(pers, 62, ' ' * 11)}")
    missing = sorted({p.split('.')[0] for p in table} - set(common))
    for port in missing:
        L.append(f"   {port + '*':6s}  없다 — 이 포트는 핀마다 다르다. 아래 표를 봐라")
    L.append("")
    L.extend(render_caps(table, wildcard))
    L.append(" 아래 표의 '이 핀만' 은 위 공통에 **더해지는** 것이다.")
    L.extend(CAPS_LEGEND)
    L.append("")

    seen = set()
    for hdr, entries in board_headers(md_path):
        has_alias = any(a for _, _, a in entries)
        aw = max([len(a) for _, _, a in entries] + [4]) if has_alias else 0
        # 이름 열은 가장 긴 것에 맞춘다. LED_BUILTIN 같은 별칭까지 붙어 길어진다.
        nw = max([len(', '.join(used.get(sig, []))) for _, sig, _ in entries] + [8])

        L.append(f" {hdr}" if hdr.endswith('헤더') or '헤더' in hdr else f" {hdr} 헤더")
        head = f"   {'Pin':>3s} {'GPIO':6s} "
        rule = f"   {'-' * 3} {'-' * 6} "
        if has_alias:
            head += f"{'Board':{aw}s} "
            rule += f"{'-' * aw} "
        head += f"{'Name in sketch':{nw}s} {'PWM':3s} {'IRQ':3s} {'ADC':4s} Only this pin"
        rule += f"{'-' * nw} {'-' * 3} {'-' * 3} {'-' * 4} {'-' * 34}"
        L.append(head)
        L.append(rule)

        for num, sig, alias in entries:
            if not re.fullmatch(r'P\d+\.\d+', sig):
                L.append(f"   {num:3d} {sig}")
                continue
            L.append(pin_row(f"{num:3d}", sig, alias if has_alias else None,
                             aw, nw, table, wildcard, used))
        L.append("")
        seen.update(sig for _, sig, _ in entries)

    # ── 헤더에 안 나오는데 variant 가 쓰는 핀 ────────────────────────────
    #
    # 온보드 LED·버튼·센서가 여기 들어간다. 빼 두면 정작 가장 많이 묻는 것
    # ("LED 에 analogWrite 되나")에 표가 답을 못 한다. XIAO 가 그랬다 —
    # 사용자 LED 가 헤더 밖 P2.00 이라 표에 아예 나오지 않았다.
    rest = [pin for pin in used if pin not in seen]
    if rest:
        nw2 = max([len(', '.join(used[pin])) for pin in rest] + [14])
        L.append(" 헤더 밖 — 온보드 부품이 쓰는 핀")
        L.append(f"       {'GPIO':6s} {'Name in sketch':{nw2}s} {'PWM':3s} {'IRQ':3s} {'ADC':4s} Only this pin")
        L.append(f"       {'-' * 6} {'-' * nw2} {'-' * 3} {'-' * 3} {'-' * 4} {'-' * 34}")
        for pin in sorted(rest, key=_pin_key):
            L.append(pin_row("   ", pin, None, 0, nw2, table, wildcard, used))
        L.append("")

    return '\n'.join(L)


def pin_row(lead, pin, alias, aw, nw, table, wildcard, used):
    """표 한 줄. 헤더 안팎에서 같은 모양이라야 눈이 안 헷갈린다."""
    row = f"   {lead} {pin:6s} "
    if alias is not None:
        row += f"{alias:{aw}s} "
    pwm, irq, ain = arduino_caps(pin, table, wildcard)
    row += f"{(', '.join(used.get(pin, [])) or ''):{nw}s} "
    row += f"{'o' if pwm else 'x':3s} {'o' if irq else 'x':3s} {(ain or 'x'):4s} "
    row += shorten(table.get(pin, []))
    return row.rstrip()


def render_chip(chip, soc, package):
    """칩 레벨 — 패키지가 내놓는 모든 핀. 보드가 무엇을 점유했는지는 모른다."""
    doc = fetch(soc, package)
    table, wildcard = pin_table(doc)
    common = port_common(wildcard)
    L = []

    L.append(f" {chip} — {package}, GPIO {len(table)}개")
    L.append("")
    L.append(" 포트 공통 — 그 포트의 어느 핀에나 붙는다")
    L.append("")
    for port, pers in common.items():
        L.append(f"   {port + '*':6s}  {wrap(pers, 62, ' ' * 11)}")
    for port in sorted({p.split('.')[0] for p in table} - set(common)):
        L.append(f"   {port + '*':6s}  없다 — 이 포트는 핀마다 다르다")
    L.append("")
    L.extend(render_caps(table, wildcard))
    L.append(" 핀마다 추가로 되는 것 (위 공통에 **더해진다**)")
    L.append("")
    for pin, labels in table.items():
        if labels:
            L.append(f"   {pin:6s} {wrap([shorten(labels)], 62, ' ' * 10)}")
    bare = [p for p, l in table.items() if not l]
    if bare:
        L.append("")
        L.append(f"   나머지는 공통뿐: {', '.join(bare)}")
    return '\n'.join(L)


def sig_ident(peripheral, signal):
    """'SPIM/SPIS00' + 'RADIO[6]' -> ['SPIM00_RADIO_6', 'SPIS00_RADIO_6']

    '/' 로 묶인 페리페럴은 같은 블록의 다른 모드라 이름이 둘이다 — 둘 다 낸다.
    대괄호 첨자는 매크로 이름에 못 쓰므로 밑줄로 편다.
    """
    sig = re.sub(r'[\[\]]', '_', signal).strip('_').replace('__', '_')
    head, _, tail = peripheral.rpartition('/')
    if not head:
        return [f"{peripheral}_{sig}"]
    num = re.search(r'\d+$', tail).group(0)
    names = [h + num for h in head.split('/')] + [tail]
    return [f"{n}_{sig}" for n in dict.fromkeys(names)]


def render_guard(chip, soc, package):
    """핀 단위 제약을 컴파일 타임 매크로로 낸다."""
    doc = fetch(soc, package)
    pins = {p['name'] for p in doc['pins'] if re.fullmatch(r'P\d+\.\d+', str(p.get('name', '')))}
    ports = sorted({int(p[1]) for p in pins})

    L = []
    for per in doc['socPeripherals']:
        for sigdef in per.get('signals', []):
            allowed = sigdef.get('allowedGpio', [])
            if not allowed:
                continue
            whole = sorted({int(a[1]) for a in allowed if a.endswith('*')})
            exact = sorted([a for a in allowed if not a.endswith('*')], key=_pin_key)

            tests, texts = [], []
            for port in whole:
                tests.append(f"NRF54L_PORT_OF(p) == {port}")
                texts.append(f"P{port} 의 아무 핀")
            for a in exact:
                port, idx = _pin_key(a)
                tests.append(f"(p) == {port * 32 + idx}")
                texts.append(a)
            if not tests:
                continue

            cond = ' || '.join(tests)
            text = ', '.join(texts)
            for name in sig_ident(per['id'], sigdef['name']):
                L.append(f"#define NRF54L_SIG_{name}(p)  ({cond})")
                L.append(f'#define NRF54L_TXT_{name}     "{per["id"]}.{sigdef["name"]} 는 {text} 만 된다"')
            L.append("")

    body = '\n'.join(L)
    return GUARD_TMPL.format(chip=chip, package=package, ports=', '.join(f'P{p}' for p in ports),
                             count=len(pins), body=body)


GUARD_TMPL = """/*
 * nrf54l_pinmap.h — 핀↔신호 제약을 컴파일 타임에 검사한다
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * ⚠ **생성된 파일이다. 손으로 고치지 마라.**
 *   `extras/gen_pinmap.py` 가 Nordic Pin Planner 의 SoC 정의에서 굽는다:
 *   https://github.com/NordicPlayground/PinPlanner
 *
 * 기준: {chip} {package} — {ports}, GPIO {count}개.
 * L05 / L10 / L15 는 제약이 **완전히 동일**하다 (JSON 을 비교해 확인).
 * LM20A 는 페리페럴이 더 많아 별도다 — M6 에서 낸다.
 *
 * ── 왜 필요한가 ───────────────────────────────────────────────────────
 *
 * `nrf54l_domains.h` 는 **포트**까지만 본다. 그것만으로는 부족하다:
 *
 *   PIN_SPI_SCK = P2.03   → 포트 검사는 통과한다 (P2 가 맞으니까)
 *                         → 그런데 SPIM00.SCK 는 P2.01·P2.06 뿐이라 동작하지 않는다
 *                         → 증상은 "SPI 가 안 된다" 뿐이고 원인이 안 보인다
 *
 * 이 헤더는 그 배정을 **빌드에서 막는다.** 오류 메시지가 쓸 수 있는 핀을 알려 준다.
 *
 * ── 쓰는 법 ───────────────────────────────────────────────────────────
 *
 *   NRF54L_ASSERT_SIG(PIN_SPI_SCK, SPIM00_SCK, "SPI SCK");
 *
 * 신호 이름은 `libraries/PinMap` 의 예제 주석 표에 있는 것 그대로다.
 * `SPIM/SPIS00` 처럼 묶여 있는 것은 `SPIM00` 과 `SPIS00` 둘 다 받는다.
 *
 * ⚠ **핀은 매크로로 넘겨라.** variant 의 `static const uint8_t D6 = ...` 같은
 *   Arduino 관용 별칭은 **C 에서 상수식이 아니라** _Static_assert 에 못 넣는다.
 *   variant.h 는 코어의 .c 들에서도 include 되므로 C 로도 컴파일된다.
 *   `_PINNUM(2, 8)` 이나 `PIN_xxx` 매크로를 써라.
 */
#ifndef _NRF54L_PINMAP_H_
#define _NRF54L_PINMAP_H_

#include "nrf54l_domains.h"

/**
 * 핀이 그 신호로 갈 수 있는지 컴파일 타임에 검사한다.
 *
 * @param pin   variant 의 핀 매크로 (절대 GPIO 번호)
 * @param sig   신호 이름 — 예: SPIM00_SCK, TWIM22_SDA, SAADC_AIN0
 * @param what  오류 메시지에 넣을 설명
 */
#define NRF54L_ASSERT_SIG(pin, sig, what) \
    NRF54L_STATIC_ASSERT(NRF54L_SIG_##sig(pin), what " : " NRF54L_TXT_##sig)

{body}
#endif /* _NRF54L_PINMAP_H_ */
"""


HEADER = """/*********************************************************************
 {title}

 ⚠ 이 파일은 손으로 쓴 것이 아니다. `extras/gen_pinmap.py` 가
   Nordic Pin Planner 의 SoC 정의에서 구워 낸다. 고칠 일이 있으면
   여기가 아니라 생성기나 출처 문서를 고쳐라.

     https://github.com/NordicPlayground/PinPlanner

 읽는 법은 이렇다. 표는 **핀 기준**이라 "P1.08 에 뭘 붙일 수 있나" 를
 바로 볼 수 있다. 반대 방향("I2C 를 어디에 붙이나")은 포트 공통 줄을
 보면 된다 — nRF54L 은 페리페럴이 도메인에 묶여 있어서, 대개
 **어느 핀이냐보다 어느 포트냐가 먼저 정해진다.**

 같은 번호의 SPIM/SPIS/TWIM/TWIS/UARTE 는 **하나의 하드웨어 블록**이다.
 TWIM20 을 쓰면 UARTE20 과 SPIM20 은 못 쓴다. 표에 셋이 나란히 보이는
 것은 "골라 쓸 수 있다" 는 뜻이지 "동시에 된다" 는 뜻이 아니다.

{body}

 baram-nrf54l-arduino - MIT license
*********************************************************************/

void setup()
{{
  Serial.begin(115200);
  delay(300);

  /* 이 스케치는 위 주석이 본체다. 굽지 않아도 된다.
     굳이 돌리면 보드 이름과 실장 칩만 확인해 준다. */
  Serial.println("{title}");
  Serial.print("빌드된 보드: ");
  Serial.println(BOARD_NAME);

  uint32_t part = *(volatile uint32_t *) 0x00FFC31C;   /* FICR INFO.PART */
  Serial.print("실장 칩 FICR INFO.PART = 0x");
  Serial.println(part, HEX);
}}

void loop()
{{
}}
"""


def write_example(dirname, title, body):
    out = os.path.join(ROOT, 'nrf54l/libraries/PinMap/examples', dirname)
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, dirname + '.ino')
    with open(path, 'w', encoding='utf-8') as f:
        f.write(HEADER.format(title=title, body=body))
    return path


CHIPS = [
    ('nRF54L05',   'nrf54l05',   'qfn48-6x6-qfaa'),
    ('nRF54L10',   'nrf54l10',   'qfn48-6x6-qfaa'),
    ('nRF54L15',   'nrf54l15',   'qfn48-6x6-qfaa'),
    ('nRF54LM20A', 'nrf54lm20a', 'qfn52-6x6-qgaa'),
]

BOARDS = [
    ('NU54-DK',        'nRF54L05', 'nrf54l05',  'qfn48-6x6-qfaa', 'NU54-DK.md',        'nu54dk'),
    ('NU54V-DK',       'nRF54L15', 'nrf54l15',  'qfn48-6x6-qfaa', 'NU54-DK.md',        'nu54dk'),
    ('XIAO_nRF54L15',  'nRF54L15', 'nrf54l15',  'qfn48-6x6-qfaa', 'XIAO-nRF54L15.md',  'xiao_nrf54l15'),
]


if __name__ == '__main__':
    guard = os.path.join(ROOT, 'nrf54l/cores/nrf54l/nrf54l_pinmap.h')
    with open(guard, 'w', encoding='utf-8') as f:
        f.write(render_guard('nRF54L15', 'nrf54l15', 'qfn52-6x6-qgaa'))
    print(' ', guard)

    for chip, soc, pkg in CHIPS:
        body = render_chip(chip, soc, pkg)
        print(' ', write_example(f'pinmap_{chip}', f'{chip} 핀맵', body))

    for board, chip, soc, pkg, md, variant in BOARDS:
        body = render_board(board, chip, soc, pkg,
                            os.path.join(ROOT, 'docs/boards', md),
                            os.path.join(ROOT, 'nrf54l/variants', variant, 'variant.h'))
        print(' ', write_example(f'pinmap_{board}', f'{board} 핀맵 ({chip})', body))
