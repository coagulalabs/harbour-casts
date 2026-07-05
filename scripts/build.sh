#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SFDK="${SFDK:-$HOME/SailfishOS/bin/sfdk}"

DO_CLEAN=0
for arg in "$@"; do
    case "$arg" in
        --clean) DO_CLEAN=1 ;;
    esac
done

cd "$ROOT"
PKG=$(awk '/^TARGET *=/{print $3; exit}' ./*.pro)

if [ $DO_CLEAN -eq 1 ]; then
    rm -f Makefile* ./*.o "$PKG" moc_*.cpp moc_*.o .qmake.stash
    rm -rf RPMS
fi

sg docker -c "$SFDK engine start" >/dev/null
sg docker -c "$SFDK build"
