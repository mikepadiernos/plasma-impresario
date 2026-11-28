#!/bin/sh
# Run the opt-in reading-to-response and composing live IMAP test against a
# disposable Dovecot. It also drives a first start from the account settings
# through triage and a reply, and runs mail rules and new-mail notifications on
# mail delivered while the application runs.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/live-imap-compose.sh [build-dir]
#
# Outgoing mail goes to a local script instead of an SMTP server; the test makes
# that script refuse a message to check how a failed send is recovered.
#
# Intended for the throwaway development container only; see
# synthetic-dovecot.sh. All users, messages and passwords are synthetic; the
# server and its data are deleted when the script exits.

set -eu

build=${1:-build}
here=$(cd "$(dirname "$0")" && pwd)
. "$here/synthetic-dovecot.sh"

trap synthetic_dovecot_stop EXIT INT TERM
synthetic_dovecot_init
synthetic_user panels
python3 "$here/seed-panel-mail.py" "$work/home/panels/Maildir"
synthetic_maildir "$work/home/panels/Maildir/.Sent"
touch "$work/home/panels/Maildir/.Sent/maildirfolder"
synthetic_maildir "$work/home/panels/Maildir/.Archive"
touch "$work/home/panels/Maildir/.Archive/maildirfolder"
synthetic_dovecot_start

status=0
QT_QPA_PLATFORM=offscreen \
    QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    QTWEBENGINE_DISABLE_SANDBOX=1 \
    PLASMA_TEST_IMAPS_PORT=$port \
    PLASMA_TEST_MAILDIR=$work/home/panels/Maildir \
    "$build/test_LiveImapCompose" || status=$?
if [ "$status" != 0 ]; then
    echo "Dovecot log:" >&2
    synthetic_dovecot_log
fi
exit "$status"
