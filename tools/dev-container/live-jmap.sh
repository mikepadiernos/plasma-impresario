#!/bin/sh
# Run the opt-in live JMAP test against a disposable Stalwart server.
#
# Usage: tools/dev-container/live-jmap.sh [build-dir]
#
# Run this on the host, not inside the development container: it starts
# Stalwart in its own container, then runs build-dir/test_LiveJmap in the
# development container sharing Stalwart's loopback interface, so JMAP is
# reached as http://localhost:8080 and SMTP as localhost:25. The users "work"
# and "other", their passwords and all messages are synthetic; the server and
# its data are deleted when the script exits.

set -eu

build=${1:-build}
here=$(cd "$(dirname "$0")" && pwd)
image=${STALWART_IMAGE:-stalwartlabs/mail-server:v0.11.8}
name=plasma-impresario-live-jmap-$$
platform=$(docker version --format '{{.Server.Os}}/{{.Server.Arch}}')
config=$(mktemp)

stop() {
    docker rm -f "$name" >/dev/null 2>&1 || true
    rm -f "$config"
}
trap stop EXIT INT TERM

cat >"$config" <<'TOML'
[server]
hostname = "localhost"

[server.listener.http]
bind = ["[::]:8080"]
protocol = "http"

[server.listener.smtp]
bind = ["[::]:25"]
protocol = "smtp"

[storage]
data = "rocksdb"
fts = "rocksdb"
blob = "rocksdb"
lookup = "rocksdb"
directory = "internal"

[store.rocksdb]
type = "rocksdb"
path = "/opt/stalwart-mail/data"
compression = "lz4"

[directory.internal]
type = "internal"
store = "rocksdb"

[authentication.fallback-admin]
user = "admin"
secret = "synthetic-admin-only"

# These synthetic logins are not addresses, which Stalwart's sender check would refuse for JMAP submissions
[session.auth]
must-match-sender = false

[tracer.stdout]
type = "stdout"
level = "warn"
ansi = false
enable = true
TOML

# The image's entry point writes a default configuration; start the server directly with ours instead
docker create --name "$name" --platform "$platform" --entrypoint /usr/local/bin/stalwart-mail \
    "$image" --config /etc/plasma-impresario-live.toml >/dev/null
docker cp -q "$config" "$name:/etc/plasma-impresario-live.toml"
docker start "$name" >/dev/null

ready=0
for _ in $(seq 1 30); do
    if docker run --rm --platform "$platform" --network "container:$name" plasma-impresario-dev \
        curl -sS -o /dev/null http://localhost:8080/.well-known/jmap 2>/dev/null; then
        ready=1
        break
    fi
    sleep 1
done
if [ "$ready" != 1 ]; then
    echo "Stalwart did not start:" >&2
    docker logs "$name" >&2
    exit 1
fi

# Accounts kept by the server itself, as in a real deployment, so their addresses become sending identities.
# Stalwart refuses identities in the reserved .test domain, hence example.com.
for principal in '{"type":"domain","name":"example.com"}' \
    '{"type":"individual","name":"work","secrets":["work-secret"],"emails":["work@example.com"],"roles":["user"]}' \
    '{"type":"individual","name":"other","secrets":["other-secret"],"emails":["other@example.com"],"roles":["user"]}'; do
    docker run --rm --platform "$platform" --network "container:$name" plasma-impresario-dev \
        curl -fsS -o /dev/null -u admin:synthetic-admin-only -H 'Content-Type: application/json' \
        -d "$principal" http://localhost:8080/api/principal
done
echo "Stalwart $(docker inspect -f '{{.Config.Image}}' "$name") is ready" >&2

status=0
"$here/run.sh" --network "container:$name" sh -c "cd /src && \
    PLASMA_TEST_JMAP_URL=http://localhost:8080 PLASMA_TEST_JMAP_SMTP_PORT=25 $build/test_LiveJmap" || status=$?
if [ "$status" != 0 ]; then
    echo "Stalwart log:" >&2
    docker logs "$name" 2>&1 | tail -n 40 >&2
fi
exit "$status"
