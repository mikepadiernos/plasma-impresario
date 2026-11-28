#!/bin/sh
# Run the opt-in live workload test against a disposable Dovecot: open a large
# INBOX, filter it, mark it read, remove a fifth of it from another client, open
# a long conversation in threads, and open messages with many attachments and
# with one large attachment. Each step records its time, the longest time the
# window stopped responding and the memory it added.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/live-imap-workloads.sh [build-dir]
#
# Sizes: PLASMA_TEST_WORKLOAD_MESSAGES (50000), PLASMA_TEST_WORKLOAD_CONVERSATION
# (500), PLASMA_TEST_WORKLOAD_ATTACHMENTS (200) and PLASMA_TEST_WORKLOAD_MIB (20).
# PLASMA_TEST_WORKLOAD_REPORT names a file that receives one tab-separated line
# per step. See docs/performance.md.
#
# Intended for the throwaway development container only; see
# synthetic-dovecot.sh. All users, messages and passwords are synthetic; the
# server and its data are deleted when the script exits.

set -eu

build=${1:-build}
here=$(cd "$(dirname "$0")" && pwd)
. "$here/synthetic-dovecot.sh"

messages=${PLASMA_TEST_WORKLOAD_MESSAGES:-50000}
conversation=${PLASMA_TEST_WORKLOAD_CONVERSATION:-500}
attachments=${PLASMA_TEST_WORKLOAD_ATTACHMENTS:-200}
mib=${PLASMA_TEST_WORKLOAD_MIB:-20}

trap synthetic_dovecot_stop EXIT INT TERM
synthetic_dovecot_init
synthetic_user workload
maildir=$work/home/workload/Maildir
python3 "$here/seed-workload-mail.py" "$maildir" "$messages" "$conversation" "$attachments" "$mib"
synthetic_dovecot_start

# Let the server index the folders first, so that the test measures the client
doveadm="doveadm -c $work/dovecot.conf"
for folder in INBOX Conversation Attachments; do
    $doveadm mailbox status -u workload@example.test messages "$folder" >/dev/null
done

status=0
QT_QPA_PLATFORM=offscreen \
    QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    QTWEBENGINE_DISABLE_SANDBOX=1 \
    PLASMA_TEST_IMAPS_PORT=$port \
    PLASMA_TEST_MAILDIR=$maildir \
    PLASMA_TEST_DOVEADM=$doveadm \
    PLASMA_TEST_WORKLOAD_MESSAGES=$messages \
    PLASMA_TEST_WORKLOAD_CONVERSATION=$conversation \
    PLASMA_TEST_WORKLOAD_ATTACHMENTS=$attachments \
    PLASMA_TEST_WORKLOAD_MIB=$mib \
    "$build/test_LiveImapWorkloads" || status=$?
if [ "$status" != 0 ]; then
    echo "Dovecot log:" >&2
    synthetic_dovecot_log
fi
exit "$status"
