# Shared helpers for a disposable, loopback-only Dovecot with synthetic users.
#
# Source this file from a script that runs inside the development container:
#
#     . tools/dev-container/synthetic-dovecot.sh
#     synthetic_dovecot_init
#     synthetic_user work
#     synthetic_dovecot_start
#
# Intended for the throwaway development container only: it installs Dovecot
# and OpenSSL when missing, creates a system user, and listens on 127.0.0.1.
# All users, messages and passwords are synthetic. Call synthetic_dovecot_stop
# (for example from an EXIT trap) to stop the server and delete its data.
#
# The generated configuration uses Dovecot 2.4 syntax, so init fails on any
# other series. It prints the exact Dovecot version so that each run records
# the server it was verified against.
#
# Variables set by synthetic_dovecot_init: work (temporary directory), port
# (PLASMA_TEST_IMAPS_PORT or 10993), password (shared by every user) and
# dovecot_version.

synthetic_dovecot_init() {
    port=${PLASMA_TEST_IMAPS_PORT:-10993}
    password=synthetic-test-only
    if ! command -v dovecot >/dev/null 2>&1 || ! command -v openssl >/dev/null 2>&1; then
        dnf -y -q install --setopt=install_weak_deps=False dovecot openssl >/dev/null
    fi
    dovecot_version=$(dovecot --version 2>/dev/null | cut -d' ' -f1)
    case "$dovecot_version" in
        2.4.*)
            echo "Synthetic Dovecot $dovecot_version on 127.0.0.1:$port" >&2
            ;;
        *)
            echo "Dovecot 2.4 is required; found ${dovecot_version:-none}" >&2
            return 1
            ;;
    esac
    id pimail >/dev/null 2>&1 || useradd -r -M -u 2525 -s /sbin/nologin pimail
    work=$(mktemp -d /tmp/pi-dovecot.XXXXXX)
    chmod 755 "$work"
    : >"$work/users"
    openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=127.0.0.1 \
        -keyout "$work/key.pem" -out "$work/cert.pem" >/dev/null 2>&1
}

synthetic_dovecot_stop() {
    if [ -n "${work-}" ] && [ -d "$work" ]; then
        dovecot -c "$work/dovecot.conf" stop >/dev/null 2>&1 || true
        rm -rf "$work"
    fi
}

synthetic_maildir() {
    for part in cur new tmp; do
        mkdir -p "$1/$part"
    done
}

# synthetic_user NAME: adds NAME@example.test with an empty INBOX at $work/home/NAME/Maildir
synthetic_user() {
    synthetic_maildir "$work/home/$1/Maildir"
    printf '%s@example.test:{PLAIN}%s:2525:2525::%s\n' "$1" "$password" "$work/home/$1" >>"$work/users"
}

# synthetic_message MAILDIR TO NAME: a minimal unread message with subject "Synthetic NAME"
synthetic_message() {
    printf 'From: Synthetic Sender <sender@example.test>\r\nTo: %s\r\nSubject: Synthetic %s\r\nDate: Tue, 24 Nov 2026 12:00:00 +0000\r\nMessage-ID: <%s@example.test>\r\n\r\nSynthetic body.\r\n' \
        "$2" "$3" "$3" >"$1/new/$3.synthetic"
}

# synthetic_dovecot_restart: stops the server and starts it again with the current
# synthetic_extra_config (extra settings appended to the generated configuration)
synthetic_dovecot_restart() {
    dovecot -c "$work/dovecot.conf" stop >/dev/null 2>&1 || true
    for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        if ! openssl s_client -connect "127.0.0.1:$port" </dev/null >/dev/null 2>&1; then
            break
        fi
        sleep 0.5
    done
    synthetic_dovecot_start
}

synthetic_dovecot_log() {
    cat "$work/dovecot.log" >&2 || true
}

synthetic_dovecot_start() {
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
${synthetic_extra_config-}
EOF

    dovecot -c "$work/dovecot.conf"
    for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        if openssl s_client -connect "127.0.0.1:$port" </dev/null >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.5
    done
    echo "Dovecot did not start; log follows" >&2
    synthetic_dovecot_log
    return 1
}
