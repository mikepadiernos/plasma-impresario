#!/bin/sh
# Run the opt-in embedded panels live IMAP test against a disposable Dovecot and
# save screenshots of the shell layouts.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/live-imap-panels.sh [build-dir] [screenshot-dir]
#
# Screenshots default to <build-dir>/shell-screenshots. They are drawn with the
# offscreen platform without Plasma's style, fonts or color scheme, so they show
# the layout but not the Plasma look.
#
# Intended for the throwaway development container only; see
# synthetic-dovecot.sh. All users, messages and passwords are synthetic; the
# server and its data are deleted when the script exits.

set -eu

build=${1:-build}
screenshots=${2:-$build/shell-screenshots}
here=$(cd "$(dirname "$0")" && pwd)
. "$here/synthetic-dovecot.sh"

trap synthetic_dovecot_stop EXIT INT TERM
synthetic_dovecot_init
synthetic_user panels
python3 "$here/seed-panel-mail.py" "$work/home/panels/Maildir"
synthetic_dovecot_start

# The offscreen platform defaults to an 800x600 screen, which squeezes the shell.
printf '{ "screens": [ { "name": "synthetic", "x": 0, "y": 0, "width": 1600, "height": 1000, "logicalDpi": 96, "dpr": 1 } ] }\n' \
    >"$work/screen.json"

rm -f "$screenshots"/*.png
status=0
QT_QPA_PLATFORM=offscreen:configfile=$work/screen.json \
    QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    QTWEBENGINE_DISABLE_SANDBOX=1 \
    PLASMA_TEST_IMAPS_PORT=$port \
    PLASMA_TEST_SCREENSHOT_DIR=$screenshots \
    "$build/test_LiveImapPanels" || status=$?
if [ "$status" != 0 ]; then
    echo "Dovecot log:" >&2
    synthetic_dovecot_log
fi
exit "$status"
