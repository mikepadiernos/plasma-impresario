#!/bin/sh
# Run the opt-in multi-account live IMAP test against a disposable Dovecot.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/live-imap-accounts.sh [build-dir]
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

for user in work home; do
    synthetic_user "$user"
done
synthetic_message "$work/home/work/Maildir" work@example.test work-first
synthetic_message "$work/home/work/Maildir" work-second@example.test work-second
synthetic_message "$work/home/home/Maildir" home@example.test home-first
synthetic_maildir "$work/home/home/Maildir/.Projects"
synthetic_dovecot_start

status=0
# One main window per process: the shortcut handler is a singleton.
for case in accountsConnectAndSwitchIndependently newProfilesKeepPasswordsOutOfSettings; do
    QT_QPA_PLATFORM=offscreen \
        QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
        QTWEBENGINE_DISABLE_SANDBOX=1 \
        PLASMA_TEST_IMAPS_PORT=$port \
        PLASMA_TEST_HOME_MAILDIR=$work/home/home/Maildir \
        "$build/test_LiveImapAccounts" "$case" || status=$?
done
if [ "$status" != 0 ]; then
    echo "Dovecot log:" >&2
    synthetic_dovecot_log
fi
exit "$status"
