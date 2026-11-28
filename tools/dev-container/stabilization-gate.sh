#!/bin/sh
# Run the M10 stabilization gate: the checks that must pass before work beyond
# M10 builds on the mail core. See docs/stabilization-gate.md.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/stabilization-gate.sh [build-dir] [ctest-args...]
#
# The build directory defaults to build. Further arguments go to ctest, for
# example -E Cryptography_PGP. PLASMA_GATE_LIVE=0 skips the live server tests,
# PLASMA_GATE_SANITIZERS=1 adds the sanitizer run of tools/dev-container/sanitizers.sh
# in build-asan, and PLASMA_GATE_JOBS sets the build and test jobs (2).

set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
build=${1:-build}
[ "$#" -gt 0 ] && shift
jobs=${PLASMA_GATE_JOBS:-2}

cd "$root"
results=
status=0

# Run one named check and remember whether it passed
check() {
    name=$1
    shift
    echo "== $name"
    if "$@"; then
        results="$results
pass  $name"
    else
        results="$results
FAIL  $name"
        status=1
    fi
}

configure_and_build() {
    cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache >/dev/null \
        && cmake --build "$build" -j "$jobs"
}

test_suite() {
    (cd "$build" && QT_QPA_PLATFORM=offscreen ctest -j "$jobs" --output-on-failure --timeout 900 "$@")
}

# The workload tests fail when a workload runs over its time budget or blocks the event loop for too long
workload_budgets() {
    for workload in test_Imap_Workloads test_SearchIndex; do
        binary=$(find "$build" -name "$workload" -type f -perm -u+x | head -n 1)
        if [ -z "$binary" ]; then
            echo "$workload was not built"
            return 1
        fi
        IMPRESARIO_WORKLOAD_BUDGETS=1 QT_QPA_PLATFORM=offscreen "$binary" || return 1
    done
}

roadmap_links() {
    python3 tools/roadmap-issue-links.py --check docs/ROADMAP.md
}

live_servers() {
    failed=0
    export PLASMA_TEST_WORKLOAD_MESSAGES=${PLASMA_TEST_WORKLOAD_MESSAGES:-5000}
    for live in live-imap-accounts live-imap-operations live-imap-panels live-imap-search live-imap-compose live-pop3 live-imap-workloads; do
        echo "-- $live"
        sh "$here/$live.sh" "$build" || { echo "-- $live failed"; failed=1; }
    done
    return "$failed"
}

sanitizers() {
    PLASMA_SANITIZER_JOBS=$jobs sh "$here/sanitizers.sh" build-asan "$@"
}

check "Build" configure_and_build
check "Test suite" test_suite "$@"
check "Workload budgets" workload_budgets
check "Roadmap issue links" roadmap_links
if [ "${PLASMA_GATE_LIVE:-1}" != 0 ]; then
    check "Live servers" live_servers
fi
if [ "${PLASMA_GATE_SANITIZERS:-0}" != 0 ]; then
    check "Sanitizers" sanitizers "$@"
fi

echo "== Stabilization gate$results"
if [ "$status" = 0 ]; then
    echo "== Passed"
else
    echo "== Failed"
fi
exit "$status"
