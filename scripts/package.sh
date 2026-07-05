#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST="$ROOT/dist"

"$SCRIPT_DIR/build.sh" "$@"

PKG=$(awk '/^TARGET *=/{print $3; exit}' "$ROOT"/*.pro)
RPM=$(ls -1 "$ROOT/RPMS/${PKG}-"*.aarch64.rpm | head -1)
mkdir -p "$DIST"
cp -f "$RPM" "$DIST/"
echo "RPM: $DIST/$(basename "$RPM")"
