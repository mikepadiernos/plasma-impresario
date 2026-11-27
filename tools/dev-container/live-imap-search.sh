#!/bin/sh
# Run the opt-in live IMAP test of searching several folders and accounts
# against a disposable Dovecot: the shown folder, every folder of the account,
# all accounts, chosen folders and the Unified Inbox, with an account whose
# password is rejected and a folder which does not exist. Searches the server
# cannot run are checked twice: with ESEARCH, then after restarting the server
# without it, so that plain SEARCH is used. The query syntax is checked against
# a third user whose messages have flags, a keyword, an attachment and the
# headers of a mailing list, which is also offered as a view and in the reader.
# The same messages are then kept offline, indexed and searched without a
# connection.
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

for user in alpha beta gamma; do
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

# gamma's INBOX, for the query syntax. Maildir flags: S seen, F flagged, a the
# first keyword of dovecot-keywords.
gamma=$work/home/gamma/Maildir
printf '0 $label1\n' >"$gamma/dovecot-keywords"
# gamma_message NAME FLAGS FROM SUBJECT DATE: a message with the rest of its
# headers and its body from stdin, already known to the server
gamma_message() {
    {
        printf 'From: %s\r\nTo: gamma@example.test\r\nSubject: %s\r\nDate: %s\r\nMessage-ID: <%s@example.test>\r\n' \
            "$3" "$4" "$5" "$1"
        cat
    } >"$gamma/cur/$1.synthetic:2,$2"
}
printf 'Content-Type: text/plain\r\n\r\nThe release plan.\r\n' |
    gamma_message release-plan Sa 'Ann Example <ann@example.test>' 'Release plan' 'Fri, 20 Nov 2026 09:00:00 +0000'
printf '\r\nSee you at noon.\r\n' |
    gamma_message lunch F 'Bob Example <bob@example.test>' 'Lunch on Friday' 'Sat, 21 Nov 2026 10:00:00 +0000'
{
    printf 'MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary="part"\r\n\r\n'
    printf -- '--part\r\nContent-Type: text/plain\r\n\r\nThe report is attached.\r\n'
    printf -- '--part\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename="report.bin"\r\n'
    printf 'Content-Transfer-Encoding: base64\r\n\r\n'
    # About 3 KB, so that size>2K finds only this message
    head -c 2400 /dev/zero | base64 | sed 's/$/\r/'
    printf -- '--part--\r\n'
} | gamma_message quarterly-report S 'Ann Example <ann@example.test>' 'Quarterly report' 'Tue, 10 Jun 2025 08:00:00 +0000'
{
    printf 'List-Id: Developers <dev.lists.example.test>\r\nList-Post: <mailto:dev@lists.example.test>\r\n'
    printf 'List-Unsubscribe: <mailto:dev-leave@lists.example.test>, <https://lists.example.test/leave>\r\n'
    printf 'List-Archive: <https://lists.example.test/archive>\r\n\r\nNotes for the release.\r\n'
} |
    gamma_message release-notes '' 'Carol Example <carol@example.test>' '[dev] Release notes' 'Sun, 22 Nov 2026 11:00:00 +0000'
synthetic_dovecot_start

status=0
QT_QPA_PLATFORM=offscreen \
    QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    QTWEBENGINE_DISABLE_SANDBOX=1 \
    PLASMA_TEST_IMAPS_PORT=$port \
    PLASMA_TEST_ALPHA_MAILDIR=$alpha \
    PLASMA_TEST_EXPECT_ESEARCH=1 \
    "$build/test_LiveImapSearch" || status=$?

if [ "$status" = 0 ]; then
    synthetic_extra_config='imap_capability {
  ESEARCH = no
}'
    synthetic_dovecot_restart
    QT_QPA_PLATFORM=offscreen \
        QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
        QTWEBENGINE_DISABLE_SANDBOX=1 \
        PLASMA_TEST_IMAPS_PORT=$port \
        PLASMA_TEST_ALPHA_MAILDIR=$alpha \
        PLASMA_TEST_EXPECT_ESEARCH=0 \
        "$build/test_LiveImapSearch" explainsSearchesTheServerCannotRun searchesWithQuerySyntax || status=$?
fi
if [ "$status" != 0 ]; then
    echo "Dovecot log:" >&2
    synthetic_dovecot_log
fi
exit "$status"
