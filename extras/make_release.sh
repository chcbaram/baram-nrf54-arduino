#!/bin/bash
#
# Package the platform and register it in the board manager index.
#
#   extras/make_release.sh <version> [--dry-run]
#
# Builds baram-nrf54l-<version>.tar.bz2 from nrf54l/, records its size and
# checksum in package_baram_nrf54_index.json, and uploads the archive as a
# GitHub release asset. The index itself is served from the main branch, so
# commit and push it afterwards.
#
# The board list is read from boards.txt rather than hardcoded, so adding a
# board needs no edit here.
#
# Tools are NOT bundled in this archive. They are declared as
# toolsDependencies and installed by Board Manager - see extras/make_tools.sh.
#
# --dry-run does everything except the upload, so the archive and the index
# entry can be inspected first.

set -euo pipefail

REPO_SLUG="chcbaram/baram-nrf54-arduino"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INDEX="$ROOT/package_baram_nrf54_index.json"
PLATFORM_DIR="$ROOT/nrf54l"

# Must match what platform.txt refers to by {runtime.tools.<name>-<version>.path}.
GCC_VERSION="14.2.1-1.1"
PROBERS_VERSION="0.32.0"

usage() {
  echo "usage: $(basename "$0") <version> [--dry-run]" >&2
  echo "  e.g. $(basename "$0") 0.1.0" >&2
  exit "${1:-1}"
}

[ $# -ge 1 ] || usage
VERSION="$1"; shift
DRY_RUN=0
for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=1 ;;
    *) echo "unknown option: $arg" >&2; usage ;;
  esac
done

# Arduino compares these as versions, so keep them strictly numeric.
if ! echo "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  echo "error: version must look like 1.2.3, got '$VERSION'" >&2
  exit 1
fi

[ -f "$PLATFORM_DIR/platform.txt" ] || { echo "error: $PLATFORM_DIR/platform.txt not found" >&2; exit 1; }
[ -f "$INDEX" ] || { echo "error: $INDEX not found" >&2; exit 1; }

# platform.txt must refer to exactly the tool versions this script declares as
# dependencies. A mismatch resolves to an empty path and upload dies with
# "fork/exec {runtime.tools....}" at the user's machine, not here.
for pair in "xpack-arm-none-eabi-gcc-$GCC_VERSION" "probe-rs-$PROBERS_VERSION"; do
  grep -q "runtime.tools.$pair.path" "$PLATFORM_DIR/platform.txt" || {
    echo "error: platform.txt does not reference runtime.tools.$pair.path" >&2
    exit 1
  }
done

# The version in platform.txt and the one in the index must agree. NU40DK let
# them drift (platform.txt 1.7.0 vs index 0.0.3); don't repeat that.
echo "==> setting platform.txt version to $VERSION"
if [ "$(uname -s)" = "Darwin" ]; then
  sed -i '' "s/^version=.*/version=$VERSION/" "$PLATFORM_DIR/platform.txt"
else
  sed -i "s/^version=.*/version=$VERSION/" "$PLATFORM_DIR/platform.txt"
fi

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

NAME="baram-nrf54l-$VERSION"
ARCHIVE="$ROOT/$NAME.tar.bz2"

echo "==> staging $PLATFORM_DIR -> $NAME/"
mkdir -p "$STAGE/$NAME"
# The archive must contain exactly one top-level directory holding platform.txt.
# tools/ is excluded on purpose: probe-rs now arrives as a tool dependency, and
# bundling it too would make every user download the same binary twice.
( cd "$PLATFORM_DIR" && tar -cf - \
    --exclude '.DS_Store' \
    --exclude '.git' \
    --exclude './tools' \
    --exclude 'platform.local.txt' \
    --exclude 'boards.local.txt' \
    . ) \
  | ( cd "$STAGE/$NAME" && tar -xf - )

echo "==> creating $(basename "$ARCHIVE")"
rm -f "$ARCHIVE"
( cd "$STAGE" && tar -cjf "$ARCHIVE" "$NAME" )

# wc -c rather than stat, whose flags differ between macOS and GNU.
SIZE="$(wc -c < "$ARCHIVE" | tr -d ' ')"
SHA256="$(shasum -a 256 "$ARCHIVE" | awk '{print toupper($1)}')"
URL="https://github.com/$REPO_SLUG/releases/download/$VERSION/$NAME.tar.bz2"

echo "    size     $SIZE"
echo "    sha256   $SHA256"
echo "    url      $URL"

echo "==> updating $(basename "$INDEX")"
VERSION="$VERSION" NAME="$NAME" URL="$URL" SIZE="$SIZE" SHA256="$SHA256" \
GCC_VERSION="$GCC_VERSION" PROBERS_VERSION="$PROBERS_VERSION" \
BOARDS_TXT="$PLATFORM_DIR/boards.txt" \
python3 - "$INDEX" <<'PY'
import json, os, re, sys

path = sys.argv[1]
with open(path) as f:
    index = json.load(f)

pkg = index["packages"][0]

# Board names come from boards.txt so this list cannot go stale. Menu options
# also match "<id>.<something>.name", hence the single-dot restriction.
boards, seen = [], set()
with open(os.environ["BOARDS_TXT"]) as f:
    for line in f:
        m = re.match(r'^([A-Za-z0-9_]+)\.name=(.+)$', line.strip())
        if m and m.group(1) not in seen:
            seen.add(m.group(1))
            boards.append({"name": m.group(2)})
if not boards:
    sys.exit("error: no boards found in boards.txt")

entry = {
    "name": "BARAM nRF54L Boards",
    "architecture": "nrf54l",
    "version": os.environ["VERSION"],
    "category": "Contributed",
    "url": os.environ["URL"],
    "archiveFileName": os.environ["NAME"] + ".tar.bz2",
    "checksum": "SHA-256:" + os.environ["SHA256"],
    "size": os.environ["SIZE"],
    "help": {"online": "https://github.com/chcbaram/baram-nrf54-arduino"},
    "boards": boards,
    # Both tools are hosted by this package, so a user only ever needs to add
    # one Board Manager URL (CLAUDE.md section 2).
    "toolsDependencies": [
        {"packager": "baram-nrf54", "name": "xpack-arm-none-eabi-gcc",
         "version": os.environ["GCC_VERSION"]},
        {"packager": "baram-nrf54", "name": "probe-rs",
         "version": os.environ["PROBERS_VERSION"]},
    ],
}

for dep in entry["toolsDependencies"]:
    tool = next((t for t in pkg.get("tools", [])
                 if t["name"] == dep["name"] and t["version"] == dep["version"]), None)
    if tool is None:
        sys.exit("error: index has no tool %s %s" % (dep["name"], dep["version"]))
    if not tool.get("systems"):
        sys.exit("error: tool %s %s has no systems; run extras/make_tools.sh first"
                 % (dep["name"], dep["version"]))

platforms = pkg.setdefault("platforms", [])
# Re-releasing the same version replaces it rather than adding a duplicate.
platforms = [p for p in platforms if p.get("version") != entry["version"]]
platforms.append(entry)
platforms.sort(key=lambda p: [int(n) for n in p["version"].split(".")])
pkg["platforms"] = platforms

with open(path, "w") as f:
    json.dump(index, f, indent=2)
    f.write("\n")

print("    boards   %s" % ", ".join(b["name"] for b in boards))
print("    %d platform entr%s in index" % (len(platforms), "y" if len(platforms) == 1 else "ies"))
PY

if [ "$DRY_RUN" -eq 1 ]; then
  echo "==> dry run, not uploading"
  echo
  echo "Archive left at $ARCHIVE"
  exit 0
fi

command -v gh >/dev/null 2>&1 || { echo "error: gh CLI not found" >&2; exit 1; }

echo "==> uploading to GitHub release $VERSION"
if gh release view "$VERSION" --repo "$REPO_SLUG" >/dev/null 2>&1; then
  gh release upload "$VERSION" "$ARCHIVE" --repo "$REPO_SLUG" --clobber
else
  gh release create "$VERSION" "$ARCHIVE" --repo "$REPO_SLUG" \
    --title "$VERSION" --notes "BARAM nRF54L Boards $VERSION"
fi

echo
echo "Done. Now commit and push package_baram_nrf54_index.json and"
echo "nrf54l/platform.txt so Board Manager picks up $VERSION."
