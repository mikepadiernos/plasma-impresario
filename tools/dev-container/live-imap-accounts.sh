#!/bin/sh
# Run the opt-in multi-account live IMAP test against a disposable Dovecot.
#
# Usage: tools/dev-container/run.sh sh tools/dev-container/live-imap-accounts.sh [build-dir]
#
# Intended for the throwaway development container only: it installs Dovecot
# and OpenSSL when missing, creates a system user, and listens on 127.0.0.1.
# All users, messages and passwords are synthetic; the server and its data are
# deleted when the script exits.

set -eu

build=${1:-build}
port=${PLASMA_TEST_IMAPS_PORT:-10993}
password=synthetic-test-only

if ! command -v dovecot >/dev/null 2>&1 || ! command -v openssl >/dev/null 2>&1; then
    dnf -y -q install --setopt=install_weak_deps=False dovecot openssl >/dev/null
fi
id pimail >/dev/null 2>&1 || useradd -r -M -u 2525 -s /sbin/nologin pimail

work=$(mktemp -d /tmp/pi-dovecot.XXXXXX)
chmod 755 "$work"
cleanup() {
    dovecot -c "$work/dovecot.conf" stop >/dev/null 2>&1 || true
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=127.0.0.1 \
    -keyout "$work/key.pem" -out "$work/cert.pem" >/dev/null 2>&1

maildir() {
    for part in cur new tmp; do
        mkdir -p "$1/$part"
    done
}

message() {
    printf 'From: Synthetic Sender <sender@example.test>\r\nTo: %s\r\nSubject: Synthetic %s\r\nDate: Tue, 24 Nov 2026 12:00:00 +0000\r\nMessage-ID: <%s@example.test>\r\n\r\nSynthetic body.\r\n' \
        "$2" "$3" "$3" >"$1/new/$3.synthetic"
}

: >"$work/users"
for user in work home; do
    maildir "$work/home/$user/Maildir"
    printf '%s@example.test:{PLAIN}%s:2525:2525::%s\n' "$user" "$password" "$work/home/$user" >>"$work/users"
done
message "$work/home/work/Maildir" work@example.test work-first
message "$work/home/work/Maildir" work-second@example.test work-second
message "$work/home/home/Maildir" home@example.test home-first
maildir "$work/home/home/Maildir/.Projects"
chown -R pimail:pimail "$work/home"

cat >"$work/dovecot.conf" <<EOF
dovecot_config_version = 2.4.0
dovecot_storage_version = 2.4.0
base_dir = $work/run
state_dir = $work/state
log_path = $work/dovecot.log
protocols = imap
listen = 127.0.0.1
mail_driver = maildir
mail_path = ~/Maildir
ssl = required
ssl_server_cert_file = $work/cert.pem
ssl_server_key_file = $work/key.pem
auth_mechanisms = plain
passdb passwd-file {
  passwd_file_path = $work/users
}
userdb passwd-file {
  passwd_file_path = $work/users
}
service imap-login {
  inet_listener imap {
    port = 0
  }
  inet_listener imaps {
    port = $port
    ssl = yes
  }
}
namespace inbox {
  inbox = yes
  separator = /
}
EOF

dovecot -c "$work/dovecot.conf"
ready=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if openssl s_client -connect "127.0.0.1:$port" </dev/null >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 0.5
done
if [ "$ready" != 1 ]; then
    echo "Dovecot did not start; log follows" >&2
    cat "$work/dovecot.log" >&2 || true
    exit 1
fi

status=0
QT_QPA_PLATFORM=offscreen \
    QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu \
    QTWEBENGINE_DISABLE_SANDBOX=1 \
    PLASMA_TEST_IMAPS_PORT=$port \
    "$build/test_LiveImapAccounts" || status=$?
if [ "$status" != 0 ]; then
    echo "Dovecot log:" >&2
    cat "$work/dovecot.log" >&2 || true
fi
exit "$status"
