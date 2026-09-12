#!/bin/bash
#
# 번들 예제를 전 보드에서 컴파일한다.
#
#   extras/check_examples.sh [보드 ...]
#
# 보드를 안 주면 boards.txt 의 전부를 돈다.
#
# ⚠ **보드 이름을 딴 라이브러리는 그 보드에서만 빌드한다.**
#   `libraries/NU54V-DK/` 처럼 보드 전용 하드웨어를 쓰는 예제는 다른 보드에서
#   당연히 컴파일되지 않는다. 예제마다 #if 가드를 넣는 대신 여기서 건너뛴다 —
#   가드는 모든 파일을 지저분하게 만들고, 제약이 있어야 할 곳은 예제가 아니라
#   "무엇을 어디서 빌드하는가" 를 아는 이 스크립트다.

# ⚠ macOS 기본 bash 는 3.2 다 — mapfile / declare -A 를 쓰지 마라.
set -o pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="baram-nrf54:nrf54l"

# 보드 전용 라이브러리 -> 그 보드의 board id (공백 구분, "라이브러리:보드")
ONLY_ON="NU54V-DK:nu54vdk"

boards="$*"
if [ -z "$boards" ]; then
  boards="$(grep -oE '^[a-z0-9_]+\.name=' "$ROOT/nrf54l/boards.txt" | sed 's/\.name=//' | sort -u | tr '\n' ' ')"
fi

fail=0
total=0

for b in $boards; do
  echo "── $b ──"
  for d in "$ROOT"/nrf54l/libraries/*/examples/*/; do
    lib="$(basename "$(dirname "$(dirname "$d")")")"

    skip=0
    for pair in $ONLY_ON; do
      if [ "$lib" = "${pair%%:*}" ] && [ "$b" != "${pair##*:}" ]; then skip=1; fi
    done
    if [ $skip -eq 1 ]; then continue; fi

    total=$((total + 1))
    out="$(arduino-cli compile -b "$PKG:$b" "$d" 2>&1)"
    if echo "$out" | grep -q "error:"; then
      echo "  ❌ $lib/$(basename "$d")"
      echo "$out" | grep "error:" | head -2 | sed 's/^/       /'
      fail=$((fail + 1))
    fi
  done
done

echo
if [ $fail -eq 0 ]; then
  echo "✅ 예제 $total 건 전부 컴파일"
else
  echo "❌ $total 건 중 $fail 건 실패"
fi
exit $((fail > 0))
