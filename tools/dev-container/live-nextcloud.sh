#!/bin/sh
# Run the opt-in live Nextcloud test against a disposable Nextcloud server.
#
# Usage: tools/dev-container/live-nextcloud.sh [build-dir]
#
# Run this on the host, not inside the development container: it starts
# Nextcloud with SQLite in its own container, then runs
# build-dir/test_LiveNextcloud in the development container sharing the
# server's loopback interface, so Nextcloud is reached as http://localhost.
# The users "work" and "other", their passwords, files and shares are
# synthetic; the server and its data are deleted when the script exits.

set -eu

build=${1:-build}
here=$(cd "$(dirname "$0")" && pwd)
image=${NEXTCLOUD_IMAGE:-nextcloud:34.0.6-apache}
name=plasma-impresario-live-nextcloud-$$
platform=$(docker version --format '{{.Server.Os}}/{{.Server.Arch}}')
work_password=work-synthetic-secret-2026
other_password=other-synthetic-secret-2026
link_password=link-synthetic-secret-2026

stop() {
    docker rm -f "$name" >/dev/null 2>&1 || true
}
trap stop EXIT INT TERM

docker run -d --name "$name" --platform "$platform" \
    -e SQLITE_DATABASE=nextcloud \
    -e NEXTCLOUD_ADMIN_USER=admin -e NEXTCLOUD_ADMIN_PASSWORD=synthetic-admin-only \
    -e NEXTCLOUD_TRUSTED_DOMAINS=localhost \
    "$image" >/dev/null

occ() {
    docker exec -u www-data "$@"
}

# The image installs Nextcloud on its first start, before the web server runs
ready=0
for _ in $(seq 1 180); do
    if docker exec "$name" curl -fsS -o /dev/null http://localhost/status.php 2>/dev/null \
        && occ "$name" php occ status --output=json 2>/dev/null | grep -q '"installed":true'; then
        ready=1
        break
    fi
    sleep 1
done
if [ "$ready" != 1 ]; then
    echo "Nextcloud did not start:" >&2
    docker logs "$name" 2>&1 | tail -n 40 >&2
    exit 1
fi

# Synthetic passwords are not checked against breach lists, which needs the internet
occ "$name" php occ app:disable password_policy >/dev/null 2>&1 || true
occ "$name" php occ background:cron >/dev/null
# New users start empty instead of with the example files, which would fill the quota
occ "$name" php occ config:system:set skeletondirectory --value= >/dev/null
occ "$name" php occ config:system:set templatedirectory --value= >/dev/null
occ -e OC_PASS="$work_password" "$name" php occ user:add --password-from-env --display-name "Work Synthetic" work >/dev/null
occ -e OC_PASS="$other_password" "$name" php occ user:add --password-from-env --display-name "Other Synthetic" other >/dev/null
# Small enough for the test to fill
occ "$name" php occ user:setting work files quota "3 MB"

# Requests as a user, from inside the server's container; prints the body
as() {
    user=$1
    password=$2
    shift 2
    docker exec "$name" curl -fsS -u "$user:$password" -H 'OCS-APIRequest: true' "$@"
}
dav=http://localhost/remote.php/dav/files
ocs=http://localhost/ocs/v2.php
json() {
    docker exec -i "$name" php -r "\$d = json_decode(stream_get_contents(STDIN), true); echo $1;"
}

# work: a folder for saved attachments, and a file with a public link protected by a password and an expiry date
as work "$work_password" -X MKCOL "$dav/work/Mail" >/dev/null
as work "$work_password" -X PUT --data-binary 'synthetic shared report' "$dav/work/Shared.pdf" >/dev/null
as work "$work_password" -d path=/Shared.pdf -d shareType=3 -d password="$link_password" -d expireDate=2099-01-01 \
    "$ocs/apps/files_sharing/api/v1/shares?format=json" >/dev/null
# work: a public link without a password, which anyone with the link can open
as work "$work_password" -X PUT --data-binary 'synthetic public notes' "$dav/work/Public.txt" >/dev/null
as work "$work_password" -d path=/Public.txt -d shareType=3 \
    "$ocs/apps/files_sharing/api/v1/shares?format=json" >/dev/null

# other: a folder shared with work for reading only, and a public link of a file of its own
as other "$other_password" -X MKCOL "$dav/other/Team" >/dev/null
as other "$other_password" -X PUT --data-binary 'synthetic team notes' "$dav/other/Team/notes.txt" >/dev/null
as other "$other_password" -d path=/Team -d shareType=0 -d shareWith=work -d permissions=1 \
    "$ocs/apps/files_sharing/api/v1/shares?format=json" >/dev/null
as other "$other_password" -X PUT --data-binary 'synthetic foreign file' "$dav/other/Foreign.pdf" >/dev/null
foreign_link=$(as other "$other_password" -d path=/Foreign.pdf -d shareType=3 \
    "$ocs/apps/files_sharing/api/v1/shares?format=json" | json '$d["ocs"]["data"]["url"]')

# App passwords, as Login Flow v2 would give: one for the test, one which it revokes
app_password=$(as work "$work_password" "$ocs/core/getapppassword?format=json" | json '$d["ocs"]["data"]["apppassword"]')
revokable_password=$(as work "$work_password" "$ocs/core/getapppassword?format=json" | json '$d["ocs"]["data"]["apppassword"]')
echo "Nextcloud $(docker inspect -f '{{.Config.Image}}' "$name") is ready" >&2

status=0
"$here/run.sh" --network "container:$name" sh -c "cd /src && \
    PLASMA_TEST_NEXTCLOUD_URL=http://localhost \
    PLASMA_TEST_NEXTCLOUD_LOGIN=work \
    PLASMA_TEST_NEXTCLOUD_APP_PASSWORD='$app_password' \
    PLASMA_TEST_NEXTCLOUD_REVOKABLE_PASSWORD='$revokable_password' \
    PLASMA_TEST_NEXTCLOUD_FOREIGN_LINK='$foreign_link' \
    QT_QPA_PLATFORM=offscreen $build/test_LiveNextcloud" || status=$?
if [ "$status" != 0 ]; then
    echo "Nextcloud log:" >&2
    docker exec "$name" sh -c 'tail -n 40 /var/www/html/data/nextcloud.log' >&2 || true
fi
exit "$status"
