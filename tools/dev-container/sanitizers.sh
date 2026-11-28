#!/bin/sh
# Build the project with AddressSanitizer and UndefinedBehaviorSanitizer, run
# the test suite, then run the opt-in live IMAP and POP3 tests against
# disposable Dovecot servers. A sanitizer report fails the test that caused it.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/sanitizers.sh [build-dir] [ctest-args...]
#
# The build directory defaults to build-asan. PLASMA_SANITIZERS changes the
# sanitizers (address;undefined), PLASMA_SANITIZER_JOBS the build jobs (2) and
# PLASMA_SANITIZER_LIVE=0 skips the live tests. See docs/sanitizers.md.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
build=${1:-build-asan}
[ "$#" -gt 0 ] && shift
sanitizers=${PLASMA_SANITIZERS:-address;undefined}

cd "$root"
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    "-DIMPRESARIO_SANITIZERS=$sanitizers" >/dev/null
# Sanitizers make the largest sources need much more memory, so a build that
# fails in parallel is finished one job at a time
cmake --build "$build" -j "${PLASMA_SANITIZER_JOBS:-2}" || cmake --build "$build" -j 1

status=0
(cd "$build" && QT_QPA_PLATFORM=offscreen ctest --output-on-failure --timeout 900 "$@") || status=$?

if [ "${PLASMA_SANITIZER_LIVE:-1}" != 0 ]; then
    # The same options as the ctest environment in CMakeLists.txt
    export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1
    export LSAN_OPTIONS="suppressions=$root/tests/lsan.supp:print_suppressions=0"
    export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:suppressions=$root/tests/ubsan.supp"
    export PLASMA_TEST_WORKLOAD_MESSAGES=${PLASMA_TEST_WORKLOAD_MESSAGES:-5000}
    for live in live-imap-accounts live-imap-operations live-imap-panels live-imap-search live-imap-compose live-pop3 live-imap-workloads; do
        echo "== $live"
        sh "$here/$live.sh" "$build" || { echo "== $live failed"; status=1; }
    done
fi
exit "$status"
