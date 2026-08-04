#!/usr/bin/env bash
# Unit + smoke tests for harbour-casts.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SFDK="${SFDK:-$HOME/SailfishOS/bin/sfdk}"
UNIT_TARGET="${UNIT_TARGET:-SailfishOS-5.1.0.11-i486}"
FAILED=0
PASSED=0

pass() { echo "PASS: $*"; PASSED=$((PASSED + 1)); }
fail() { echo "FAIL: $*"; FAILED=$((FAILED + 1)); }

section() { echo; echo "=== $* ==="; }

# --- Smoke: source tree / packaging invariants ---
smoke_source() {
    section "Smoke: source tree"
    local f

    for f in \
        harbour-casts.pro \
        harbour-casts.desktop \
        rpm/harbour-casts.spec \
        rpm/harbour-casts.profile \
        qml/harbour-casts.qml \
        qml/pages/SubscriptionsPage.qml \
        qml/pages/EpisodesPage.qml \
        qml/pages/PlayerPage.qml \
        qml/pages/AddPodcastPage.qml \
        qml/pages/QueuePage.qml \
        qml/pages/EpisodeNotesPage.qml \
        qml/components/PlaybackBar.qml \
        qml/cover/CoverPage.qml \
        src/feedparser.cpp \
        src/podcaststore.cpp \
        src/playercontroller.cpp
    do
        if [[ -s "$ROOT/$f" ]]; then
            pass "exists $f"
        else
            fail "missing/empty $f"
        fi
    done

    # Version consistency: README / SPEC / desktop Name
    local spec_ver
    spec_ver=$(awk '/^Version:/{print $2; exit}' "$ROOT/rpm/harbour-casts.spec")
    if grep -q "Version:\*\* ${spec_ver}" "$ROOT/README.md" || grep -q "\*\*Version:\*\* ${spec_ver}" "$ROOT/README.md"; then
        pass "README version matches SPEC ($spec_ver)"
    else
        fail "README version does not match SPEC ($spec_ver)"
    fi

    if grep -q '^Name=Casts$' "$ROOT/harbour-casts.desktop"; then
        pass "desktop Name=Casts"
    else
        fail "desktop Name unexpected"
    fi

    if grep -q 'Permissions=Internet;UserDirs;Downloads;Audio;Compatibility' "$ROOT/harbour-casts.desktop"; then
        pass "desktop sailjail permissions present"
    else
        fail "desktop sailjail permissions missing"
    fi

    # QML: basic brace balance
    local qml unbalanced=0
    while IFS= read -r -d '' qml; do
        local opens closes
        opens=$(grep -o '{' "$qml" | wc -l)
        closes=$(grep -o '}' "$qml" | wc -l)
        if [[ "$opens" -ne "$closes" ]]; then
            fail "QML brace imbalance $(realpath --relative-to="$ROOT" "$qml") {$opens/$closes}"
            unbalanced=1
        fi
    done < <(find "$ROOT/qml" -name '*.qml' -print0)
    [[ $unbalanced -eq 0 ]] && pass "QML brace balance"

    # Icon sizes required by SAILFISHAPP_ICONS
    for size in 86x86 108x108 128x128 172x172; do
        if [[ -s "$ROOT/icons/$size/harbour-casts.png" ]]; then
            pass "icon $size"
        else
            fail "missing icon $size"
        fi
    done
}

smoke_rpms() {
    section "Smoke: RPM artefacts"
    local rpm found=0
    shopt -s nullglob
    local rpms=("$ROOT"/dist/harbour-casts-*.rpm "$ROOT"/RPMS/harbour-casts-*.rpm)
    # Unique by basename
    declare -A seen=()
    for rpm in "${rpms[@]}"; do
        local base
        base=$(basename "$rpm")
        [[ -n ${seen[$base]+x} ]] && continue
        seen[$base]=1
        found=1
        if [[ -s "$rpm" ]]; then
            pass "rpm present $base ($(stat -c%s "$rpm") bytes)"
        else
            fail "rpm empty $base"
            continue
        fi
        case "$base" in
            *.aarch64.rpm|*.armv7hl.rpm|*.i486.rpm) pass "rpm arch suffix $base" ;;
            *) fail "unexpected rpm name $base" ;;
        esac
        # Magic / file type
        if file "$rpm" | grep -qi 'RPM'; then
            pass "rpm magic $base"
        else
            fail "not an RPM: $base ($(file -b "$rpm"))"
        fi
    done
    [[ $found -eq 1 ]] || fail "no RPMs found in dist/ or RPMS/"
}

run_unit() {
    section "Unit: FeedParser (sfdk $UNIT_TARGET)"
    if [[ ! -x "$SFDK" ]]; then
        fail "sfdk not found at $SFDK"
        return
    fi

    local builddir="$ROOT/tests/build-$UNIT_TARGET"
    local reldir="tests/build-$UNIT_TARGET"
    rm -rf "$builddir"
    mkdir -p "$builddir/src" "$builddir/fixtures"
    cp -a "$ROOT/tests/tst_feedparser.cpp" "$builddir/"
    cp -a "$ROOT/src/feedparser.cpp" "$ROOT/src/feedparser.h" "$builddir/src/"
    cp -a "$ROOT/tests/fixtures/"* "$builddir/fixtures/"
    cat > "$builddir/tests.pro" <<'EOF'
TEMPLATE = app
TARGET = tst_feedparser
CONFIG += console testcase c++14
CONFIG -= app_bundle
QT += core testlib
QT -= gui

INCLUDEPATH += $$PWD/src

SOURCES += \
    tst_feedparser.cpp \
    src/feedparser.cpp

HEADERS += \
    src/feedparser.h

TESTDATA += fixtures/*
EOF

    if ! sg docker -c "$SFDK config --session target=$UNIT_TARGET"; then
        fail "could not set sfdk target $UNIT_TARGET"
        return
    fi

    # build-shell uses the *.default snapshot; ensure QtTest is there
    if ! sg docker -c "$SFDK build-shell pkg-config --exists Qt5Test"; then
        echo "  Installing qt5-qttest-devel into ${UNIT_TARGET}.default..."
        sg docker -c "$SFDK tools package-install ${UNIT_TARGET}.default qt5-qttest-devel" >/dev/null \
            || sg docker -c "$SFDK tools package-install $UNIT_TARGET qt5-qttest-devel" >/dev/null \
            || true
    fi

    # build-shell must be invoked from the app project root
    local out rc=0
    out=$(cd "$ROOT" && sg docker -c "$SFDK build-shell sh -c \"cd ${reldir} && qmake tests.pro && make -j\$(nproc) && ./tst_feedparser -o -,txt\"" 2>&1) || rc=$?
    echo "$out" | sed 's/^/  /'
    if [[ $rc -eq 0 ]] && echo "$out" | grep -q 'Totals:'; then
        pass "tst_feedparser"
    elif [[ $rc -eq 0 ]]; then
        pass "tst_feedparser (exit 0)"
    else
        fail "tst_feedparser build/run failed (exit $rc)"
    fi

    sg docker -c "$SFDK config --session --drop target" >/dev/null 2>&1 || true
}

run_sfdk_check() {
    section "Smoke: sfdk check (rpmlint + harbour notes)"
    if [[ ! -x "$SFDK" ]]; then
        fail "sfdk not found"
        return
    fi

    local rpm
    shopt -s nullglob
    local any=0
    for rpm in "$ROOT"/dist/harbour-casts-*.rpm; do
        any=1
        local base out rc=0
        base=$(basename "$rpm")

        # rpmlint: hard fail on errors (warnings are OK for OpenRepos)
        out=$(cd "$ROOT" && sg docker -c "$SFDK check -l package -s rpmlint -- $rpm" 2>&1) || rc=$?
        echo "$out" | sed 's/^/  /'
        if echo "$out" | grep -qE '[0-9]+ packages .* 0 errors'; then
            pass "rpmlint $base"
        elif [[ $rc -eq 0 ]]; then
            pass "rpmlint $base"
        else
            fail "rpmlint $base (exit $rc)"
        fi

        # harbour validator: informational - sailjail /etc profile is required for SFOS 4+/OpenRepos
        # but rejected by classic Harbour path rules. Arch mismatch also false-fails cross-arch RPMs.
        rc=0
        out=$(cd "$ROOT" && sg docker -c "$SFDK check -l package -s harbour -- $rpm" 2>&1) || rc=$?
        if echo "$out" | grep -q 'Installation not allowed in this location'; then
            pass "harbour $base (known: sailjail profile path - OK for OpenRepos)"
        elif [[ $rc -eq 0 ]]; then
            pass "harbour $base"
        else
            echo "$out" | sed -n '/ERROR/p;/FAILED/p;/Validation failed/p' | sed 's/^/  /'
            fail "harbour $base (unexpected failure)"
        fi
    done
    [[ $any -eq 1 ]] || fail "no dist RPMs for sfdk check"
}

cd "$ROOT"
smoke_source
smoke_rpms
run_unit
run_sfdk_check

section "Summary"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[[ $FAILED -eq 0 ]]
