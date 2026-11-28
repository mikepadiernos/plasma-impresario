#!/bin/sh
# Run the M11 release gate on the checked-out commit: formatting, supported
# dependency versions, a reproducible rebuild, the test suite with coverage, and
# static analysis. See docs/release-gate.md.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/release-gate.sh [ctest-args...]
#
# Arguments go to ctest, for example -E Cryptography_PGP. Builds go to
# build-release-cov and build-release-repro, and the reports to
# build-release-gate. PLASMA_RELEASE_JOBS sets the build, test and analysis
# jobs (3). PLASMA_RELEASE_REPRO=0, PLASMA_RELEASE_COVERAGE=0 or
# PLASMA_RELEASE_TIDY=0 skips a check; the gate then reports itself incomplete.

set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
data=$root/tools/release
jobs=${PLASMA_RELEASE_JOBS:-3}
cov=build-release-cov
repro=build-release-repro
out=build-release-gate

cd "$root"
mkdir -p "$out"
results=
status=0
incomplete=0

check() {
    check_name=$1
    shift
    echo "== $check_name"
    if "$@"; then
        results="$results
pass  $check_name"
    else
        results="$results
FAIL  $check_name"
        status=1
    fi
}

skip() {
    echo "== $1 (skipped)"
    results="$results
skip  $1"
    incomplete=1
}

# Every C++ file outside the baseline must already be formatted
formatting() {
    : > "$out/format.txt"
    baseline=$(grep -v '^#' "$data/format-baseline.txt")
    checked=0
    failed=0
    tidied=0
    for f in $(git ls-files 'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h'); do
        if printf '%s\n' "$baseline" | grep -Fxq "$f"; then
            if clang-format --style="file:$data/clang-format" --dry-run -Werror "$f" >/dev/null 2>&1; then
                echo "formatted, remove from the baseline: $f" >> "$out/format.txt"
                tidied=$((tidied + 1))
            fi
            continue
        fi
        checked=$((checked + 1))
        if ! clang-format --style="file:$data/clang-format" --dry-run -Werror "$f" >/dev/null 2>&1; then
            echo "not formatted: $f" >> "$out/format.txt"
            failed=$((failed + 1))
        fi
    done
    echo "$checked files checked, $failed not formatted; $(printf '%s\n' "$baseline" | wc -l) in the baseline, $tidied of them now formatted" | tee -a "$out/format.txt"
    grep '^not formatted' "$out/format.txt"
    [ "$failed" = 0 ]
}

configure_coverage() {
    cmake -S . -B "$cov" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_FLAGS="--coverage -O0" -DCMAKE_C_FLAGS="--coverage -O0" \
        -DCMAKE_EXE_LINKER_FLAGS=--coverage -DCMAKE_SHARED_LINKER_FLAGS=--coverage \
        -DCMAKE_MODULE_LINKER_FLAGS=--coverage > "$out/configure.txt"
}

# CMake refuses versions below the declared minimums; this records what was found and checks again
dependencies() {
    configure_coverage || return 1
    cp "$cov/dependency-versions.txt" "$out/dependency-versions.txt"
    failed=0
    while IFS="$(printf '\t')" read -r package found minimum; do
        case "$package" in '#'*|'') continue ;; esac
        if [ "$minimum" != - ] && [ "$(printf '%s\n%s\n' "$minimum" "$found" | sort -V | head -n 1)" != "$minimum" ]; then
            echo "unsupported: $package $found, needs $minimum"
            failed=1
        else
            printf '%-26s %-12s %s\n' "$package" "$found" "$minimum"
        fi
    done < "$out/dependency-versions.txt"
    [ "$failed" = 0 ]
}

# Builds the application twice in the same directory without ccache and compares every binary
reproducible() {
    epoch=$(git log -1 --format=%ct)
    rm -f "$out/repro-a.txt" "$out/repro-b.txt"
    for pass in a b; do
        rm -rf "$repro"
        CCACHE_DISABLE=1 SOURCE_DATE_EPOCH=$epoch QT_RCC_SOURCE_DATE_OVERRIDE=$epoch \
            cmake -S . -B "$repro" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
            -DCMAKE_C_COMPILER_LAUNCHER= -DCMAKE_CXX_COMPILER_LAUNCHER= > /dev/null || return 1
        CCACHE_DISABLE=1 SOURCE_DATE_EPOCH=$epoch QT_RCC_SOURCE_DATE_OVERRIDE=$epoch \
            cmake --build "$repro" -j "$jobs" > "$out/repro-build-$pass.txt" 2>&1 || { tail -n 20 "$out/repro-build-$pass.txt"; return 1; }
        (cd "$repro" && find . -type f \( -perm -u+x -o -name '*.so*' \) ! -path './CMakeFiles/*' -exec sha256sum {} + | sort -k 2) > "$out/repro-$pass.txt"
    done
    count=$(wc -l < "$out/repro-a.txt")
    if [ "$(cat "$out/repro-a.txt")" = "$(cat "$out/repro-b.txt")" ]; then
        echo "$count binaries identical in both builds"
        return 0
    fi
    echo "binaries that differ between the builds:"
    sort "$out/repro-a.txt" "$out/repro-b.txt" | uniq -u | awk '{print $2}' | sort -u
    return 1
}

coverage() {
    cmake --build "$cov" -j "$jobs" > "$out/coverage-build.txt" 2>&1 || { tail -n 20 "$out/coverage-build.txt"; return 1; }
    find "$cov" -name '*.gcda' -delete
    (cd "$cov" && QT_QPA_PLATFORM=offscreen ctest -j "$jobs" --timeout 900 --output-on-failure "$@") > "$out/tests.txt" 2>&1
    tests=$?
    grep -E 'tests passed|tests failed|\(Failed\)|\(Timeout\)|\*\*\*' "$out/tests.txt"
    gcovr --root . --object-directory "$cov" --filter 'src/' --exclude '.*/qwwsmtpclient/.*' \
        -j "$jobs" --gcov-ignore-parse-errors=negative_hits.warn_once_per_file --gcov-ignore-errors=no_working_dir_found \
        --txt "$out/coverage.txt" --print-summary > "$out/coverage-summary.txt" 2>"$out/coverage-errors.txt"
    cat "$out/coverage-summary.txt"
    [ "$tests" = 0 ]
}

# Correctness checks only; style checks are left to clang-format
tidy_checks='-*,clang-analyzer-core.*,clang-analyzer-cplusplus.*,clang-analyzer-unix.*,clang-analyzer-optin.cplusplus.VirtualCall,bugprone-use-after-move,bugprone-dangling-handle,bugprone-undefined-memory-manipulation,bugprone-infinite-loop,bugprone-unhandled-self-assignment'

# Fails on findings that are not in the reviewed baseline
static_analysis() {
    [ -f "$cov/compile_commands.json" ] || configure_coverage || return 1
    [ -d "$cov/src" ] || cmake --build "$cov" -j "$jobs" > "$out/coverage-build.txt" 2>&1 || return 1
    run-clang-tidy -p "$cov" -j "$jobs" -quiet -checks="$tidy_checks" \
        -extra-arg=-Wno-unknown-warning-option -extra-arg=-Wno-unused-command-line-argument \
        "^$root/src/" > "$out/clang-tidy-raw.txt" 2>&1
    # One line per finding: file, check and message, without the line number, which moves with edits
    grep -E '^/.*: (warning|error): .*\[[A-Za-z0-9.,-]+\]$' "$out/clang-tidy-raw.txt" \
        | sed -e "s|^$root/||" -e 's|^\([^:]*\):[0-9]*:[0-9]*: [a-z]*: \(.*\) \[\([^]]*\)\]$|\1\t\3\t\2|' \
        | sort -u > "$out/clang-tidy.txt"
    grep -v '^#' "$data/clang-tidy-baseline.txt" | sort -u > "$out/clang-tidy-baseline.txt"
    comm -23 "$out/clang-tidy.txt" "$out/clang-tidy-baseline.txt" > "$out/clang-tidy-new.txt"
    comm -13 "$out/clang-tidy.txt" "$out/clang-tidy-baseline.txt" > "$out/clang-tidy-gone.txt"
    echo "$(wc -l < "$out/clang-tidy.txt") findings, $(wc -l < "$out/clang-tidy-new.txt") not in the baseline, $(wc -l < "$out/clang-tidy-gone.txt") baseline entries no longer found"
    cut -f 2 "$out/clang-tidy.txt" | sort | uniq -c | sort -rn
    cat "$out/clang-tidy-new.txt"
    [ ! -s "$out/clang-tidy-new.txt" ]
}

{
    echo "commit $(git rev-parse HEAD)$(git diff --quiet HEAD || echo ' with local changes')"
    grep -E '^(NAME|VERSION_ID)=' /etc/os-release
    uname -m
    for tool in cmake ninja g++ clang-format clang-tidy gcovr; do
        printf '%s: ' "$tool"
        "$tool" --version 2>&1 | grep -m 1 -i version
    done
} > "$out/environment.txt"
cat "$out/environment.txt"

check "Formatting" formatting
check "Dependency versions" dependencies
if [ "${PLASMA_RELEASE_REPRO:-1}" != 0 ]; then
    check "Reproducible build" reproducible
else
    skip "Reproducible build"
fi
if [ "${PLASMA_RELEASE_COVERAGE:-1}" != 0 ]; then
    check "Tests with coverage" coverage "$@"
else
    skip "Tests with coverage"
fi
if [ "${PLASMA_RELEASE_TIDY:-1}" != 0 ]; then
    check "Static analysis" static_analysis
else
    skip "Static analysis"
fi

echo "== Release gate$results" | tee "$out/summary.txt"
if [ "$status" != 0 ]; then
    echo "== Failed" | tee -a "$out/summary.txt"
    exit 1
fi
if [ "$incomplete" != 0 ]; then
    echo "== Incomplete" | tee -a "$out/summary.txt"
    exit 2
fi
echo "== Passed" | tee -a "$out/summary.txt"
