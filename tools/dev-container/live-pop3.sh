#!/bin/sh
# Run the opt-in live POP3 test against a disposable Dovecot.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/live-pop3.sh [build-dir]
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

for user in work drop; do
    synthetic_user "$user"
done
synthetic_message "$work/home/work/Maildir" work@example.test work-first
synthetic_message "$work/home/work/Maildir" work@example.test work-second
synthetic_message "$work/home/drop/Maildir" drop@example.test drop-first
synthetic_message "$work/home/drop/Maildir" drop@example.test drop-second
synthetic_pop3=1
synthetic_dovecot_start
echo "POP3 on 127.0.0.1:$pop3_port (STLS) and 127.0.0.1:$pop3s_port (TLS)" >&2

status=0
PLASMA_TEST_POP3_PORT=$pop3_port \
    PLASMA_TEST_POP3S_PORT=$pop3s_port \
    PLASMA_TEST_POP3_CERT=$work/cert.pem \
    PLASMA_TEST_POP3_HOME=$work/home \
    "$build/test_LivePop3" || status=$?
if [ "$status" != 0 ]; then
    echo "Dovecot log:" >&2
    synthetic_dovecot_log
fi
exit "$status"
