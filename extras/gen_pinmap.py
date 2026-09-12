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
    """variant.h 가 이미 잡아 둔 핀. 'P1.13' -> 'PIN_BUTTON1'."""
    text = open(variant_h, encoding='utf-8').read()
    used = {}
    for m in re.finditer(r'#define\s+(PIN_\w+|LED_\w+)\s+_PINNUM\(\s*(\d+)\s*,\s*(\d+)\s*\)', text):
        name, port, idx = m.group(1), int(m.group(2)), int(m.group(3))
        used.setdefault(f"P{port}.{idx:02d}", []).append(name)
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
    L.append(" 아래 표의 '이 핀만' 은 위 공통에 **더해지는** 것이다.")
    L.append("")

    for hdr, entries in board_headers(md_path):
        has_alias = any(a for _, _, a in entries)
        aw = max([len(a) for _, _, a in entries] + [4]) if has_alias else 0

        L.append(f" {hdr}" if hdr.endswith('헤더') or '헤더' in hdr else f" {hdr} 헤더")
        head = f"   {'핀':>2s}  {'GPIO':6s} "
        rule = f"   {'-' * 3} {'-' * 6} "
        if has_alias:
            head += f"{'보드 이름':{aw}s} "
            rule += f"{'-' * aw} "
        head += f"{'variant':20s} 이 핀만"
        rule += f"{'-' * 20} {'-' * 40}"
        L.append(head)
        L.append(rule)

        for num, sig, alias in entries:
            if not re.fullmatch(r'P\d+\.\d+', sig):
                L.append(f"   {num:3d} {sig}")
                continue
            row = f"   {num:3d} {sig:6s} "
            if has_alias:
                row += f"{alias:{aw}s} "
            row += f"{(', '.join(used.get(sig, [])) or ''):20s} {shorten(table.get(sig, []))}"
            L.append(row.rstrip())
        L.append("")
    return '\n'.join(L)


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
    for chip, soc, pkg in CHIPS:
        body = render_chip(chip, soc, pkg)
        print(' ', write_example(f'pinmap_{chip}', f'{chip} 핀맵', body))

    for board, chip, soc, pkg, md, variant in BOARDS:
        body = render_board(board, chip, soc, pkg,
                            os.path.join(ROOT, 'docs/boards', md),
                            os.path.join(ROOT, 'nrf54l/variants', variant, 'variant.h'))
        print(' ', write_example(f'pinmap_{board}', f'{board} 핀맵 ({chip})', body))
