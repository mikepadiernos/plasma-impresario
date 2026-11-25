#!/bin/sh
# Run Plasma Impresario on a virtual X display that can be viewed over VNC,
# connected to a disposable Dovecot seeded with the embedded panel messages.
#
# Usage (from the host): tools/dev-container/vnc.sh [build-dir]
# Inside the container:   sh tools/dev-container/vnc-session.sh [build-dir]
#
# This is plain Qt on Xvfb with the Openbox window manager, not a Plasma
# session: the style, fonts, color scheme, KWin behavior and desktop services
# differ from Plasma. Use it to try layouts, menus and divider dragging.
#
# Everything is synthetic and disposable: the mail server, its account
# (panels@example.test), the application's settings and the VNC password are
# created in a temporary directory and deleted when the application quits.

set -eu

build=${1:-build}
vnc_port=${VNC_PORT:-5901}
vnc_password=synthvnc
here=$(cd "$(dirname "$0")" && pwd)
. "$here/synthetic-dovecot.sh"

if [ ! -x "$build/plasma-impresario" ]; then
    echo "$build/plasma-impresario is missing; build it first with tools/dev-container/run.sh cmake --build $build" >&2
    exit 1
fi

missing=
for tool in Xvfb x11vnc openbox; do
    command -v "$tool" >/dev/null 2>&1 || missing=1
done
if [ -n "$missing" ]; then
    echo "Installing the virtual display packages; rebuild the image with run.sh --rebuild to keep them." >&2
    dnf -y -q install --setopt=install_weak_deps=False xorg-x11-server-Xvfb x11vnc openbox \
        xcb-util-cursor libxkbcommon-x11 mesa-dri-drivers >/dev/null
fi

xvfb_pid=
vnc_pid=
wm_pid=
cleanup() {
    for pid in $wm_pid $vnc_pid $xvfb_pid; do
        kill "$pid" 2>/dev/null || true
    done
    synthetic_dovecot_stop
}
trap cleanup EXIT INT TERM

synthetic_dovecot_init
synthetic_user panels
python3 "$here/seed-panel-mail.py" "$work/home/panels/Maildir"
synthetic_dovecot_start

# Application settings with the synthetic account and its cleartext password.
app_home=$work/app-home
config=$app_home/.config/flaska.net
mkdir -p "$config"
mkdir -m 700 "$work/runtime"
# Trust the disposable server certificate up front, as if it had been accepted
# once, because new dialogs can stay black on this display (see CONTRIBUTING.md).
pubkey=$(openssl x509 -in "$work/cert.pem" -pubkey -noout | awk '{ printf "%s\\n", $0 }')
cat >"$config/trojita.conf" <<EOF
[accounts]
ids=account-1
active=account-1
next=2
names\\account-1=Synthetic panels

[accountPasswords]
account-1\\imap=$password

[gui]
showSystray=false

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
EOF

Xvfb :1 -screen 0 1600x1000x24 -nolisten tcp >"$work/xvfb.log" 2>&1 &
xvfb_pid=$!
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    [ -S /tmp/.X11-unix/X1 ] && break
    sleep 0.25
done
if [ ! -S /tmp/.X11-unix/X1 ]; then
    echo "Xvfb did not start; log follows" >&2
    cat "$work/xvfb.log" >&2
    exit 1
fi

x11vnc -storepasswd "$vnc_password" "$work/vnc.passwd" >/dev/null 2>&1
# Listens on every container interface so that Docker can publish the port;
# run.sh --publish exposes it on the host's loopback interface only.
x11vnc -display :1 -rfbport "$vnc_port" -rfbauth "$work/vnc.passwd" -forever -shared \
    -noxdamage -quiet -o "$work/x11vnc.log" &
vnc_pid=$!
DISPLAY=:1 openbox >"$work/openbox.log" 2>&1 &
wm_pid=$!

echo "VNC: vnc://localhost:$vnc_port  password: $vnc_password"
echo "Quit the application to stop."

status=0
HOME=$app_home \
    XDG_CONFIG_HOME=$app_home/.config \
    XDG_DATA_HOME=$app_home/.local/share \
    XDG_CACHE_HOME=$app_home/.cache \
    XDG_RUNTIME_DIR=$work/runtime \
    DISPLAY=:1 \
    QT_QPA_PLATFORM=xcb \
    QT_X11_NO_MITSHM=1 \
    QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    QTWEBENGINE_DISABLE_SANDBOX=1 \
    dbus-run-session -- "$build/plasma-impresario" || status=$?
exit "$status"
