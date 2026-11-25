#!/bin/sh
# Start the application in the development container on a virtual display and
# make it viewable over VNC from this machine.
#
# Usage: tools/dev-container/vnc.sh [build-dir]
#
# Then open vnc://localhost:5901 (on macOS: Finder > Go > Connect to Server,
# or Screen Sharing) with the password synthvnc. Set VNC_PORT to use another
# port. The port is published on the loopback interface only. Quit the
# application, or press Ctrl+C here, to stop. See vnc-session.sh.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
port=${VNC_PORT:-5901}
exec "$here/run.sh" --publish "$port" env VNC_PORT="$port" sh tools/dev-container/vnc-session.sh "$@"
