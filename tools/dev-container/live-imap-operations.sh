#!/bin/sh
# Run the opt-in destructive mail operations live IMAP test against a disposable
# Dovecot: Move to Trash and Undo, Delete Permanently, Delete without Trash and
# Expunge, Empty Trash, Empty Junk and Rename Folder.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/live-imap-operations.sh [build-dir]
#
# The test checks each action against the server's Maildir files, not only
# against what the window shows.
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
synthetic_user operations
maildir=$work/home/operations/Maildir
address=operations@example.test
for name in keep trash purge expunge; do
    synthetic_message "$maildir" "$address" "$name"
done
for folder in Trash Junk Projects; do
    synthetic_maildir "$maildir/.$folder"
    touch "$maildir/.$folder/maildirfolder"
done
synthetic_message "$maildir/.Trash" "$address" old-1
synthetic_message "$maildir/.Trash" "$address" old-2
synthetic_message "$maildir/.Junk" "$address" spam-1
synthetic_message "$maildir/.Junk" "$address" spam-2
synthetic_message "$maildir/.Projects" "$address" plan
synthetic_dovecot_start

status=0
QT_QPA_PLATFORM=offscreen \
    QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    QTWEBENGINE_DISABLE_SANDBOX=1 \
    PLASMA_TEST_IMAPS_PORT=$port \
    PLASMA_TEST_MAILDIR=$maildir \
    "$build/test_LiveImapOperations" || status=$?
if [ "$status" != 0 ]; then
    echo "Dovecot log:" >&2
    synthetic_dovecot_log
fi
exit "$status"
