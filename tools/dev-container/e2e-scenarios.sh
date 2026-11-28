#!/bin/sh
# Install Plasma Impresario and drive it through AT-SPI and the keyboard in a
# headless X session against a synthetic Dovecot: setup, reading, hostile mail,
# search, delete and undo, compose and send, the tray icon, notifications and a
# restart. Each scenario leaves screenshots and a report line.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/e2e-scenarios.sh [build-dir] [scenario,...]
#
# The build directory (build) must already be built; it is installed into the
# container's prefix, which run.sh throws away afterwards. Screenshots, the
# reports and the application's output go to build-e2e/ (PLASMA_E2E_OUT).
#
# This is plain Qt on Xvfb with Openbox, a fake notification server and a fake
# tray host from tools/e2e/desktop_mocks.py, not a Plasma session. The renderer
# sandbox is off because the container has no user namespaces. Every user,
# address and message is synthetic, on example.test, and deleted afterwards.

set -eu

build=${1:-build}
only=${2:-}
out=${PLASMA_E2E_OUT:-build-e2e}
display=${PLASMA_E2E_DISPLAY:-:5}
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
. "$here/synthetic-dovecot.sh"

if [ ! -x "$build/plasma-impresario" ]; then
    echo "$build/plasma-impresario is missing; build it first with tools/dev-container/run.sh cmake --build $build" >&2
    exit 1
fi
for tool in Xvfb openbox xdotool /usr/libexec/at-spi-bus-launcher; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool is missing; rebuild the image with tools/dev-container/run.sh --rebuild" >&2
        exit 1
    fi
done

xvfb_pid=
wm_pid=
cleanup() {
    for pid in $wm_pid $xvfb_pid; do
        kill "$pid" 2>/dev/null || true
    done
    synthetic_dovecot_stop
}
trap cleanup EXIT INT TERM

# The installed application, with its notification events and desktop file
cmake --install "$build" >/dev/null
app=$(sed -n 's/^CMAKE_INSTALL_PREFIX:PATH=//p' "$build/CMakeCache.txt")/bin/plasma-impresario

synthetic_dovecot_init
synthetic_user panels
maildir=$work/home/panels/Maildir
python3 "$here/seed-panel-mail.py" "$maildir"
for folder in Sent Trash Archive; do
    synthetic_maildir "$maildir/.$folder"
    touch "$maildir/.$folder/maildirfolder"
done
chown -R 2525:2525 "$work/home"
synthetic_dovecot_start

# A profile with the synthetic account, the tray icon on, and a sendmail stand-in
profile=$work/profile
config=$profile/.config/flaska.net
mkdir -p "$config"
mkdir -m 700 "$work/runtime"
sent=$work/sent.eml
printf "cat > '%s'\n" "$sent" >"$work/sendmail.sh"
pubkey=$(openssl x509 -in "$work/cert.pem" -pubkey -noout | awk '{ printf "%s\\n", $0 }')
cat >"$config/trojita.conf" <<EOF
[accounts]
ids=account-1
active=account-1
next=2
names\\account-1=Example Mail

[accountPasswords]
account-1\\imap=$password

[gui]
showSystray=true

[plugin]
password=cleartextpassword
EOF
cat >"$config/trojita-account-1.conf" <<EOF
[General]
imap.method=SSL
imap.host=127.0.0.1
imap.port=$port
imap.auth.user=panels@example.test
imap.ssl.pemPubKey="@ByteArray($pubkey)"
imap.startmode=ONLINE
imap.needsNetwork=false
imap.proxy.system=false
offline.cache=memory
msa.method=sendmail
msa.sendmail=/bin/sh $work/sendmail.sh

[composer]
saveToImapEnabled=true
imapSentName=Sent

[identities]
1\\realName=Panels User
1\\address=panels@example.test
size=1
EOF

rm -rf "$out"
mkdir -p "$out"
out=$(cd "$out" && pwd)

Xvfb "$display" -screen 0 1600x1000x24 -nolisten tcp >"$out/xvfb.log" 2>&1 &
xvfb_pid=$!
socket=/tmp/.X11-unix/X${display#:}
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    [ -S "$socket" ] && break
    sleep 0.25
done
[ -S "$socket" ] || { echo "Xvfb did not start" >&2; cat "$out/xvfb.log" >&2; exit 1; }
DISPLAY=$display openbox >"$out/openbox.log" 2>&1 &
wm_pid=$!

echo "Application: $app ($("$app" --version 2>/dev/null | tail -n 1))"
status=0
env DISPLAY="$display" \
    XDG_RUNTIME_DIR="$work/runtime" \
    QT_X11_NO_MITSHM=1 \
    QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    QTWEBENGINE_DISABLE_SANDBOX=1 \
    E2E_APP="$app" \
    E2E_HOME="$work/app-home" \
    E2E_PROFILE="$profile" \
    E2E_MAILDIR="$maildir" \
    E2E_MOCK_LOG="$out/desktop-events.jsonl" \
    E2E_SENT="$sent" \
    E2E_IMAP_PORT="$port" \
    E2E_USER=panels@example.test \
    E2E_PASSWORD="$password" \
    dbus-run-session -- sh -c '
        /usr/libexec/at-spi-bus-launcher --launch-immediately >"$0/at-spi.log" 2>&1 &
        : >"$E2E_MOCK_LOG"
        python3 "$1/tools/e2e/desktop_mocks.py" "$E2E_MOCK_LOG" &
        for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
            grep -q ready "$E2E_MOCK_LOG" && break
            sleep 0.25
        done
        sleep 1
        python3 "$1/tools/e2e/scenarios.py" ${2:+--only "$2"} "$0"
    ' "$out" "$root" "$only" || status=$?
echo "Report: $out/report.md"
if [ "$status" != 0 ]; then
    echo "Dovecot log:" >&2
    synthetic_dovecot_log | tail -n 20 >&2
fi
exit "$status"
