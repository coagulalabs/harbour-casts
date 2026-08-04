#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST="$ROOT/dist"
SFDK="${SFDK:-$HOME/SailfishOS/bin/sfdk}"

# Optional: TARGETS="aarch64 armv7hl" ./scripts/package.sh
TARGETS="${TARGETS:-aarch64}"
DO_CLEAN=0
for arg in "$@"; do
    case "$arg" in
        --clean) DO_CLEAN=1 ;;
        --both) TARGETS="aarch64 armv7hl" ;;
    esac
done

resolve_target() {
    case "$1" in
        aarch64) echo "${SFDK_TARGET_AARCH64:-SailfishOS-5.0.0.62-aarch64}" ;;
        armv7hl) echo "${SFDK_TARGET_ARMV7HL:-SailfishOS-5.1.0.11-armv7hl}" ;;
        *) echo "unknown arch: $1" >&2; return 1 ;;
    esac
}

mkdir -p "$DIST"
PKG=$(awk '/^TARGET *=/{print $3; exit}' "$ROOT"/*.pro)

for arch in $TARGETS; do
    target=$(resolve_target "$arch")
    echo "==> Building $arch ($target)"
    sg docker -c "$SFDK config --session target=$target"
    # Keep RPM version clean even with local uncommitted edits.
    sg docker -c "$SFDK config --session no-fix-version"

    if [ $DO_CLEAN -eq 1 ]; then
        (cd "$ROOT" && rm -f Makefile* ./*.o "$PKG" moc_*.cpp moc_*.o .qmake.stash documentation.list)
        rm -rf "$ROOT/RPMS"
    fi

    (cd "$ROOT" && sg docker -c "$SFDK build")

    RPM=$(ls -1t "$ROOT"/RPMS/${PKG}-*."${arch}".rpm 2>/dev/null | head -1 || true)
    if [ -z "$RPM" ]; then
        echo "No ${arch} RPM produced" >&2
        exit 1
    fi
    cp -f "$RPM" "$DIST/"
    echo "RPM: $DIST/$(basename "$RPM")"
done

sg docker -c "$SFDK config --session --drop target" >/dev/null 2>&1 || true
sg docker -c "$SFDK config --session --drop no-fix-version" >/dev/null 2>&1 || true
