#!/usr/bin/env python3
"""
보드 핀아웃 그림 생성기 — baram-nrf54l-arduino
SPDX-License-Identifier: MIT

`docs/boards/<보드>.md` 의 확장 헤더 표를 읽어 아두이노 공식 핀아웃 같은
SVG 를 만든다. **핀 데이터를 손으로 옮기지 않는다** — 문서가 정본이고
(CLAUDE.md §4.1) 그림은 거기서 파생된다. 표를 고치면 다시 돌리면 된다.

SVG 인 이유: GitHub 이 마크다운에서 그대로 렌더하고, 벡터라 확대해도 깨지지
않으며, 텍스트라 diff 가 읽힌다. PNG 는 셋 다 안 된다.

    python3 extras/gen_pinout_svg.py
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'extras'))
from gen_pinmap import board_headers            # 표 파서를 공유한다

# ── 기능별 색 ────────────────────────────────────────────────────────
# 아두이노/Adafruit 핀아웃의 관례를 따른다: 전원은 빨강·검정, 통신은
# 채도 높은 색, 범용 GPIO 는 눈에 안 띄는 색.
C = {
    'pwr':    ('#c62828', '#ffffff'),   # 전원
    'gnd':    ('#37474f', '#ffffff'),   # GND
    'gpio':   ('#eceff1', '#263238'),   # 범용
    'analog': ('#1565c0', '#ffffff'),   # ADC
    'i2c':    ('#2e7d32', '#ffffff'),   # I2C
    'spi':    ('#6a1b9a', '#ffffff'),   # SPI
    'uart':   ('#ef6c00', '#ffffff'),   # UART
    'led':    ('#00838f', '#ffffff'),   # 온보드 LED / 버튼
    'taken':  ('#9e9e9e', '#ffffff'),   # 보드가 이미 쓰는 핀
    'nc':     ('#ffffff', '#b0bec5'),   # 패드는 있으나 연결되지 않음
    'unk':    ('#cfd8dc', '#546e7a'),   # 도면에서 확정 못 한 것
}

def classify(name, gpio):
    n = (name or '').lower()
    g = (gpio or '').upper()
    if g in ('SWDCLK', 'SWDIO', 'MOD_RST'):                        return 'taken'
    if gpio in ('VBUS', 'VEXT', 'VBAT', 'VDD_3V3_SYS', 'VDD_MOD'): return 'pwr'
    if gpio == 'GND':                                              return 'gnd'
    if 'spi' in n:                                                 return 'spi'
    if 'qwiic' in n or 'sda' in n or 'scl' in n:                   return 'i2c'
    if 'serial' in n:                                              return 'uart'
    if 'swd' in n or 'rst' in n or 'swo' in n:                     return 'taken'
    if 'pmic' in n or 'vbat' in n:                                 return 'taken'
    if 'led' in n or n.startswith('sw'):                           return 'led'
    if re.match(r'^a\d', n):                                       return 'analog'
    if not n or n == '—':                                          return 'gpio'
    return 'gpio'

def esc(s):
    return (s or '').replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


# ── 치수 ─────────────────────────────────────────────────────────────
PITCH   = 26          # 핀 간격
CHIP_H  = 20
NUM_W   = 30          # 핀 번호 칸
GPIO_W  = 80          # P1.07 / VDD_3V3_SYS 가 들어가야 한다
FN_W    = 150         # 기능 이름 칸
BOARD_W = 250
GAP     = 14
MARGIN  = 24
TOP     = 120         # 제목 영역


def chip(x, y, w, text, kind, anchor='middle'):
    bg, fg = C[kind]
    tx = x + w / 2 if anchor == 'middle' else x + 8
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{CHIP_H}" rx="4" fill="{bg}"/>'
            f'<text x="{tx}" y="{y + CHIP_H/2 + 4}" text-anchor="{anchor}" '
            f'font-family="ui-monospace,Menlo,Consolas,monospace" font-size="11" '
            f'fill="{fg}">{esc(text)}</text>')


def render(board, chip_name, md_path, power, note, nc=None, order=None):
    nc = nc or {}
    order = order or {}
    """power: {헤더이름: {핀번호: 넷}} — 표에 없는 전원 핀을 채운다.
    nc:    {헤더이름: {핀번호: 사유}} — 패드는 있으나 미연결인 자리.
    order: {헤더이름: [위에서 아래로 그릴 핀 번호]} — 실물 배치를 따른다."""
    hdrs = board_headers(md_path)
    rows = {}
    for name, entries in hdrs:
        key = name.split()[0]
        rows[key] = {n: (gpio, alias) for n, gpio, alias in entries}

    left_key, right_key = 'P2', 'P4'
    for k, extra in power.items():
        rows.setdefault(k, {})
        for n, net in extra.items():
            rows[k][n] = (net, '')

    n_pins = 30
    side_w = NUM_W + GPIO_W + FN_W + 2 * 4
    W = MARGIN * 2 + side_w * 2 + GAP * 2 + BOARD_W
    H = TOP + n_pins * PITCH + 150

    s = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
         f'viewBox="0 0 {W} {H}" font-family="system-ui,-apple-system,sans-serif">',
         f'<rect width="{W}" height="{H}" fill="#ffffff"/>',
         f'<text x="{MARGIN}" y="40" font-size="24" font-weight="700" fill="#263238">{esc(board)}</text>',
         f'<text x="{MARGIN}" y="64" font-size="13" fill="#546e7a">{esc(chip_name)} · 확장 헤더 핀아웃</text>',
         f'<text x="{MARGIN}" y="84" font-size="11" fill="#b71c1c">{esc(note)}</text>']

    bx = MARGIN + side_w + GAP
    s.append(f'<rect x="{bx}" y="{TOP - 10}" width="{BOARD_W}" height="{n_pins * PITCH + 20}" '
             f'rx="10" fill="#1b5e20" opacity="0.08" stroke="#1b5e20" stroke-opacity="0.35"/>')
    s.append(f'<text x="{bx + BOARD_W/2}" y="{TOP + 26}" text-anchor="middle" font-size="15" '
             f'font-weight="600" fill="#1b5e20">{esc(board)}</text>')
    for i, (label, sub) in enumerate([
            ('USB-C + 온보드 CMSIS-DAP', '시리얼 포트 2개로 잡힌다'),
            ('Qwiic (J5)', 'P1.02 SDA / P1.03 SCL · 2.1K 풀업'),
            ('BQ25186 충전기', 'Wire 버스의 0x6A · 배터리 전압은 A5'),
            ('J1 점퍼', '빼고 전류계를 물리면 모듈 전류만 측정'),
            ('LED1~4 / SW1~4', 'LED active HIGH · 버튼은 내부 풀업')]):
        y = TOP + 70 + i * 46
        s.append(f'<text x="{bx + 16}" y="{y}" font-size="12" font-weight="600" fill="#1b5e20">{esc(label)}</text>')
        s.append(f'<text x="{bx + 16}" y="{y + 16}" font-size="10" fill="#37474f">{esc(sub)}</text>')

    for side, key in (('L', left_key), ('R', right_key)):
        data = rows.get(key, {})
        hx = MARGIN if side == 'L' else bx + BOARD_W + GAP
        s.append(f'<text x="{hx if side=="L" else hx + side_w}" y="{TOP + 4}" '
                 f'text-anchor="{"start" if side=="L" else "end"}" font-size="13" '
                 f'font-weight="700" fill="#263238">{esc(key)} 헤더</text>')
        # ⚠ 헤더마다 번호가 도는 방향이 다르다. 실물 NU54V-DK 는 P2 가 1번이
        #   위, P4 는 **30번이 위**다 — 회로도도 그렇게 그려져 있다.
        for row, n in enumerate(order.get(key, range(1, n_pins + 1))):
            y = TOP + 16 + row * PITCH
            gpio, alias = data.get(n, ('', ''))
            kind = classify(alias, gpio) if gpio else 'unk'
            label = alias if alias and alias != '—' else ''
            if not label and kind in ('pwr', 'gnd', 'taken'):
                label = gpio
            if not gpio:
                gpio, label, kind = '', 'GND / 전원', 'unk'
            if n in nc.get(key, ()):
                # 패드는 있지만 솔더 브리지가 없어 MCU 에 닿지 않는다.
                gpio, label, kind = '', nc[key][n], 'nc'
            if side == 'L':
                x = hx
                s.append(chip(x, y, NUM_W, str(n), 'gpio'))
                s.append(chip(x + NUM_W + 4, y, GPIO_W, gpio, kind))
                s.append(chip(x + NUM_W + GPIO_W + 8, y, FN_W, label, kind, 'start'))
            else:
                x = hx + side_w
                s.append(chip(x - NUM_W, y, NUM_W, str(n), 'gpio'))
                s.append(chip(x - NUM_W - 4 - GPIO_W, y, GPIO_W, gpio, kind))
                s.append(chip(x - NUM_W - GPIO_W - 8 - FN_W, y, FN_W, label, kind, 'start'))

    ly = TOP + n_pins * PITCH + 40
    s.append(f'<text x="{MARGIN}" y="{ly}" font-size="12" font-weight="700" fill="#263238">범례</text>')
    for i, (k, t) in enumerate([('pwr','전원'), ('gnd','GND'), ('i2c','I2C'), ('uart','UART'),
                                ('spi','SPI'), ('analog','아날로그'), ('led','온보드 LED/버튼'),
                                ('taken','보드가 점유'), ('gpio','범용 GPIO'), ('nc','미연결')]):
        x = MARGIN + (i % 4) * 195
        y = ly + 14 + (i // 4) * 26
        s.append(chip(x, y, 26, '', k))
        s.append(f'<text x="{x + 34}" y="{y + 14}" font-size="11" fill="#37474f">{esc(t)}</text>')

    s.append('</svg>')
    return '\n'.join(s)


if __name__ == '__main__':
    # 표에 없는 전원 핀. P4 는 도면 이미지로 확인했고, P2 는 확정하지 못했다.
    # 전원/GND 는 실물 통전 확인으로 확정했다 (2026-09-12). 도면 텍스트로는
    # 두 전원 버스가 갈리지 않아 한동안 비워 두었던 자리다.
    P2_GND = [1, 2, 3, 4, 5, 6, 7, 8, 13, 14, 15, 18, 30]
    P4_GND = [1, 2, 13, 18, 23, 24, 28]
    POWER = {'P4': {n: 'GND' for n in P4_GND},
             'P2': {n: 'GND' for n in P2_GND}}
    POWER['P4'].update({30: 'VBUS', 29: 'VEXT', 27: 'VBAT',
                        26: 'VDD_3V3_SYS', 25: 'VDD_MOD', 3: 'MOD_RST'})
    POWER['P2'].update({27: 'SWDCLK', 28: 'SWDIO', 29: 'VDD_MOD'})
    NC = {'P4': {14: '미연결 — SB20 미실장', 15: '미연결 — SB21 미실장'}}
    # 실물 배치: P2 는 1번이 위, P4 는 30번이 위다.
    ORDER = {'P2': list(range(1, 31)), 'P4': list(range(30, 0, -1))}
    svg = render('NU54V-DK', 'nRF54L15',
                 os.path.join(ROOT, 'docs/boards/NU54V-DK.md'), POWER,
                 '회로도에서 생성 · 전원/GND 는 실물 통전 확인 · '
                 'P4 는 도면과 18행 일치 · P2 핀 번호는 실크스크린 미대조', NC, ORDER)
    out = os.path.join(ROOT, 'docs/boards/NU54V-DK-pinout.svg')
    open(out, 'w', encoding='utf-8').write(svg)
    print(' ', out)
