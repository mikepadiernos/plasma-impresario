#!/bin/sh
# Run a command in the headless Linux development container.
#
# Usage: tools/dev-container/run.sh [--rebuild] [--publish PORT] [--network NET] [command [args...]]
#
# Without a command, this opens an interactive shell. The repository is mounted
# at /src, and ccache data persists in a named Docker volume. --publish makes the
# container's PORT reachable as PORT on the host's loopback interface only.
# --network joins a Docker network, such as container:NAME to share a server's
# loopback interface.
#
# Set EXTRA_CA_CERT to a PEM file when a TLS-intercepting proxy requires an
# extra root certificate. NODE_EXTRA_CA_CERTS is used when EXTRA_CA_CERT is unset.

set -eu

image=plasma-impresario-dev
ccache_volume=plasma-impresario-ccache
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

rebuild=0
publish=
network=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --rebuild)
            rebuild=1
            shift
            ;;
        --publish)
            publish=${2:?--publish needs a port}
            shift 2
            ;;
        --network)
            network=${2:?--network needs a network}
            shift 2
            ;;
        *)
            break
            ;;
    esac
done

platform=$(docker version --format '{{.Server.Os}}/{{.Server.Arch}}')

if [ "$rebuild" = 1 ] || ! docker image inspect "$image" >/dev/null 2>&1; then
    ca=${EXTRA_CA_CERT:-${NODE_EXTRA_CA_CERTS:-}}
    if [ -n "$ca" ] && [ -f "$ca" ]; then
        docker build --platform "$platform" --secret "id=extra_ca,src=$ca" \
            -t "$image" -f "$here/Containerfile" "$here"
    else
        docker build --platform "$platform" \
            -t "$image" -f "$here/Containerfile" "$here"
    fi
fi

if [ "$#" -eq 0 ]; then
    set -- bash
fi

tty_flags=-i
if [ -t 0 ] && [ -t 1 ]; then
    tty_flags=-it
fi

exec docker run --rm "$tty_flags" \
    --platform "$platform" \
    --shm-size=1g \
    ${publish:+-p "127.0.0.1:$publish:$publish"} \
    ${network:+--network "$network"} \
    -v "$root:/src" \
    -v "$ccache_volume:/ccache" \
    "$image" "$@"
