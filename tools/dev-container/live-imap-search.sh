#!/bin/sh
# Run the opt-in live IMAP test of searching several folders and accounts
# against a disposable Dovecot: the shown folder, every folder of the account,
# all accounts, chosen folders and the Unified Inbox, with an account whose
# password is rejected and a folder which does not exist.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/live-imap-search.sh [build-dir]
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

for user in alpha beta; do
    synthetic_user "$user"
done
alpha=$work/home/alpha/Maildir
beta=$work/home/beta/Maildir
# Maildir++ folders; the server shows them as Archive, Archive/2024 and Receipts
for folder in "$alpha/.Archive" "$alpha/.Archive.2024" "$beta/.Receipts"; do
    synthetic_maildir "$folder"
    touch "$folder/maildirfolder"
done
synthetic_message "$alpha" alpha@example.test invoice-alpha-inbox
synthetic_message "$alpha" alpha@example.test lunch
synthetic_message "$alpha/.Archive" alpha@example.test invoice-alpha-archive
synthetic_message "$alpha/.Archive.2024" alpha@example.test invoice-alpha-old
synthetic_message "$beta" beta@example.test invoice-beta
synthetic_message "$beta/.Receipts" beta@example.test invoice-beta-receipt
synthetic_dovecot_start

status=0
QT_QPA_PLATFORM=offscreen \
    QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    QTWEBENGINE_DISABLE_SANDBOX=1 \
    PLASMA_TEST_IMAPS_PORT=$port \
    PLASMA_TEST_ALPHA_MAILDIR=$alpha \
    "$build/test_LiveImapSearch" || status=$?
if [ "$status" != 0 ]; then
    echo "Dovecot log:" >&2
    synthetic_dovecot_log
fi
exit "$status"
