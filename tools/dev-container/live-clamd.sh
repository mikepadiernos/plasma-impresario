#!/bin/sh
# Run the opt-in live ClamAV test: scan files through a real clamd with the
# INSTREAM command, as opening an attachment does when scanning is turned on.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/live-clamd.sh [build-dir]
#
# clamd loads only a one-line test signature written here, so no signature
# database is downloaded and the run says nothing about detecting real malware.
# It listens on a socket in a temporary folder; both are removed when the
# script exits.

set -eu

build=${1:-build}
clamd=$(command -v clamd || echo /usr/sbin/clamd)
if [ ! -x "$clamd" ]; then
    echo "clamd is not installed; rebuild the image with tools/dev-container/run.sh --rebuild" >&2
    exit 1
fi

work=$(mktemp -d)
pid=
stop() {
    if [ -n "$pid" ]; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    fi
    rm -rf "$work"
}
trap stop EXIT INT TERM

# A body signature for an ASCII marker; clamd reports it as
# Plasma.Test.Marker.UNOFFICIAL.
marker=PLASMA-IMPRESARIO-CLAMD-TEST-MARKER
mkdir "$work/db"
hex=$(printf '%s' "$marker" | od -An -tx1 | tr -d ' \n')
printf 'Plasma.Test.Marker:0:*:%s\n' "$hex" >"$work/db/plasma-test.ndb"
cat >"$work/clamd.conf" <<EOF
LocalSocket $work/clamd.sock
DatabaseDirectory $work/db
TemporaryDirectory $work
Foreground yes
LogFile $work/clamd.log
LogVerbose yes
StreamMaxLength 1M
EOF

"$clamd" --version
"$clamd" -c "$work/clamd.conf" &
pid=$!
tries=0
while [ ! -S "$work/clamd.sock" ]; do
    tries=$((tries + 1))
    if [ "$tries" -gt 300 ] || ! kill -0 "$pid" 2>/dev/null; then
        echo "clamd did not start:" >&2
        cat "$work/clamd.log" >&2 || true
        exit 1
    fi
    sleep 0.1
done

status=0
PLASMA_TEST_CLAMD_SOCKET=$work/clamd.sock \
    PLASMA_TEST_CLAMD_MARKER=$marker \
    "$build/test_ClamdScanner" liveClamd || status=$?
if [ "$status" != 0 ]; then
    echo "clamd log:" >&2
    cat "$work/clamd.log" >&2 || true
fi
exit "$status"
