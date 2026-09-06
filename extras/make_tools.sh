#!/bin/bash
#
# Repackage probe-rs for Board Manager and register it in the index.
#
#   extras/make_tools.sh [<probe-rs version>] [--dry-run]
#
# Why repackage instead of pointing at the upstream release:
#
#   - platform.txt uses {runtime.tools.probe-rs-<ver>.path}/bin/probe-rs, but
#     the upstream archives put the binary at the top level, and the Windows
#     zip has no top-level directory at all.
#   - upstream ships .tar.xz; .tar.bz2 / .zip are what Arduino indexes use.
#   - the Linux build is not stripped. It is 136 MB, of which 94 MB is DWARF
#     debug info. Stripping needs an ELF-aware strip, so this script uses one
#     when it can find it and warns when it cannot.
#
# Run this only when the probe-rs version changes. The archive and the checksum
# in the index have to come from the SAME run, so --dry-run leaves the archives
# in place for inspection but the real run is what the index must describe.

set -euo pipefail

REPO_SLUG="chcbaram/baram-nrf54-arduino"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INDEX="$ROOT/package_baram_nrf54_index.json"

VERSION="0.32.0"
DRY_RUN=0
for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=1 ;;
    -*) echo "unknown option: $arg" >&2; exit 1 ;;
    *) VERSION="$arg" ;;
  esac
done

[ -f "$INDEX" ] || { echo "error: $INDEX not found" >&2; exit 1; }

# Arduino host triple  <tab>  upstream asset name
# There is no upstream probe-rs build for 32-bit anything, nor for armv7 Linux,
# so arm-linux-gnueabihf can compile but not upload. i686-mingw32 is listed
# because that is the host arduino-cli looks for first on Windows.
HOSTS="
arm64-apple-darwin	probe-rs-tools-aarch64-apple-darwin.tar.xz
x86_64-apple-darwin	probe-rs-tools-x86_64-apple-darwin.tar.xz
x86_64-pc-linux-gnu	probe-rs-tools-x86_64-unknown-linux-gnu.tar.xz
aarch64-linux-gnu	probe-rs-tools-aarch64-unknown-linux-gnu.tar.xz
i686-mingw32	probe-rs-tools-x86_64-pc-windows-msvc.zip
x86_64-mingw32	probe-rs-tools-x86_64-pc-windows-msvc.zip
"

UPSTREAM="https://github.com/probe-rs/probe-rs/releases/download/v$VERSION"
TAG="probe-rs-$VERSION"
OUT="$ROOT/dist/tools"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"

# An ELF-aware strip. macOS /usr/bin/strip cannot touch ELF, so fall back
# quietly rather than corrupting the binary.
elf_strip() {
  local f="$1" t
  for t in llvm-strip llvm-objcopy strip; do
    command -v "$t" >/dev/null 2>&1 || continue
    case "$t" in
      llvm-strip)   "$t" --strip-debug "$f" 2>/dev/null && return 0 ;;
      llvm-objcopy) "$t" --strip-debug "$f" 2>/dev/null && return 0 ;;
      strip)        [ "$(uname -s)" = "Linux" ] && "$t" --strip-debug "$f" 2>/dev/null && return 0 ;;
    esac
  done
  return 1
}

ENTRIES="$WORK/entries.json"
echo "[]" > "$ENTRIES"

echo "$HOSTS" | while IFS=$'\t' read -r HOST ASSET; do
  [ -n "${HOST:-}" ] || continue

  # The two Windows hosts share one upstream asset; build it once.
  OUTNAME="probe-rs-$VERSION-$(echo "$ASSET" | sed -e 's/^probe-rs-tools-//' -e 's/\.tar\.xz$//' -e 's/\.zip$//')"
  case "$ASSET" in
    *.zip) EXT="zip" ;;
    *)     EXT="tar.bz2" ;;
  esac
  BUILT="$OUT/$OUTNAME.$EXT"

  if [ ! -f "$BUILT" ]; then
    echo "==> $HOST"
    echo "    fetching $ASSET"
    curl -fsSL --retry 3 -o "$WORK/$ASSET" "$UPSTREAM/$ASSET"

    rm -rf "$WORK/x"; mkdir -p "$WORK/x"
    case "$ASSET" in
      *.zip) unzip -qo "$WORK/$ASSET" -d "$WORK/x" ;;
      *)     tar xf "$WORK/$ASSET" -C "$WORK/x" ;;
    esac

    BIN="$(find "$WORK/x" -type f \( -name 'probe-rs' -o -name 'probe-rs.exe' \) | head -1)"
    [ -n "$BIN" ] || { echo "error: probe-rs binary not found in $ASSET" >&2; exit 1; }

    BEFORE="$(wc -c < "$BIN" | tr -d ' ')"
    case "$ASSET" in
      *linux*) elf_strip "$BIN" || echo "    WARN: no ELF-aware strip found; shipping unstripped ($((BEFORE/1048576)) MB). Install llvm or run on Linux." ;;
    esac
    AFTER="$(wc -c < "$BIN" | tr -d ' ')"
    [ "$BEFORE" = "$AFTER" ] || echo "    stripped $((BEFORE/1048576)) MB -> $((AFTER/1048576)) MB"

    # Lay the tool out the way platform.txt expects: <tool>/bin/probe-rs.
    rm -rf "$WORK/stage"; mkdir -p "$WORK/stage/probe-rs-$VERSION/bin"
    cp "$BIN" "$WORK/stage/probe-rs-$VERSION/bin/$(basename "$BIN")"
    chmod +x "$WORK/stage/probe-rs-$VERSION/bin/$(basename "$BIN")"

    rm -f "$BUILT"
    if [ "$EXT" = "zip" ]; then
      ( cd "$WORK/stage" && zip -qr "$BUILT" "probe-rs-$VERSION" )
    else
      ( cd "$WORK/stage" && tar -cjf "$BUILT" "probe-rs-$VERSION" )
    fi
  else
    echo "==> $HOST (reusing $(basename "$BUILT"))"
  fi

  SIZE="$(wc -c < "$BUILT" | tr -d ' ')"
  SHA256="$(shasum -a 256 "$BUILT" | awk '{print toupper($1)}')"
  echo "    $(basename "$BUILT")  $SIZE  $SHA256"

  HOST="$HOST" FILE="$(basename "$BUILT")" SIZE="$SIZE" SHA256="$SHA256" \
  URL="https://github.com/$REPO_SLUG/releases/download/$TAG/$(basename "$BUILT")" \
  python3 - "$ENTRIES" <<'PY'
import json, os, sys
p = sys.argv[1]
e = json.load(open(p))
e.append({
    "host": os.environ["HOST"],
    "url": os.environ["URL"],
    "archiveFileName": os.environ["FILE"],
    "checksum": "SHA-256:" + os.environ["SHA256"],
    "size": os.environ["SIZE"],
})
json.dump(e, open(p, "w"))
PY
done

echo "==> updating $(basename "$INDEX")"
VERSION="$VERSION" python3 - "$INDEX" "$ENTRIES" <<'PY'
import json, os, sys
index_path, entries_path = sys.argv[1], sys.argv[2]
index = json.load(open(index_path))
systems = json.load(open(entries_path))
if not systems:
    sys.exit("error: no tool archives were built")
pkg = index["packages"][0]
tools = pkg.setdefault("tools", [])
tool = next((t for t in tools if t["name"] == "probe-rs" and t["version"] == os.environ["VERSION"]), None)
if tool is None:
    tool = {"name": "probe-rs", "version": os.environ["VERSION"], "systems": []}
    tools.append(tool)
tool["systems"] = systems
with open(index_path, "w") as f:
    json.dump(index, f, indent=2)
    f.write("\n")
print("    %d systems for probe-rs %s" % (len(systems), os.environ["VERSION"]))
PY

if [ "$DRY_RUN" -eq 1 ]; then
  echo "==> dry run, not uploading"
  echo
  echo "Archives left in $OUT"
  echo "NOTE: the checksums now in the index describe THESE files. Upload them,"
  echo "      or re-run without --dry-run so both come from the same build."
  exit 0
fi

command -v gh >/dev/null 2>&1 || { echo "error: gh CLI not found" >&2; exit 1; }

echo "==> uploading to GitHub release $TAG"
if ! gh release view "$TAG" --repo "$REPO_SLUG" >/dev/null 2>&1; then
  gh release create "$TAG" --repo "$REPO_SLUG" --title "probe-rs $VERSION" \
    --notes "probe-rs $VERSION repackaged for Arduino Board Manager. Upstream: https://github.com/probe-rs/probe-rs/releases/tag/v$VERSION"
fi
gh release upload "$TAG" "$OUT"/probe-rs-"$VERSION"-* --repo "$REPO_SLUG" --clobber

echo
echo "Done. Commit and push package_baram_nrf54_index.json."
