#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="$(mktemp -d)"
mkdir "${tmp}/bin" "${tmp}/limited" "${tmp}/home"
trap 'rm -rf -- "${tmp}"' EXIT

cat > "${tmp}/bin/cmake" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" >> "${INSTALL_TEST_CALLS}"
if [[ "${1:-}" == "-S" && "${FAIL_CONFIGURE:-false}" == "true" ]]; then
  exit 19
fi
EOF
cat > "${tmp}/bin/pacman" <<'EOF'
#!/usr/bin/env bash
printf 'pacman %s\n' "$*" >> "${INSTALL_TEST_CALLS}"
if [[ "${1:-}" == "-Syu" && "${INSTALL_SUCCEEDS:-false}" == "true" ]]; then
  exit 0
fi
if [[ "${1:-}" == "-Qq" ]]; then
  [[ "${2:-}" != "mimetic" && "${2:-}" != "${MISSING_REQUIRED:-}" ]]
  exit
fi
if [[ "${1:-}" == "-Si" && "${2:-}" == "mimetic" && "${OPTIONAL_AVAILABLE:-false}" == "true" ]]; then
  exit 0
fi
exit 1
EOF
cat > "${tmp}/bin/sudo" <<'EOF'
#!/usr/bin/env bash
exec "$@"
EOF
cat > "${tmp}/bin/dnf" <<'EOF'
#!/usr/bin/env bash
printf 'dnf %s\n' "$*" >> "${INSTALL_TEST_CALLS}"
[[ "${1:-}" == "list" && "${2:-}" == "installed" ]]
EOF
cat > "${tmp}/bin/kbuildsycoca6" <<'EOF'
#!/usr/bin/env bash
printf 'kbuildsycoca6\n' >> "${INSTALL_TEST_CALLS}"
EOF
chmod +x "${tmp}/bin/cmake" "${tmp}/bin/pacman" "${tmp}/bin/dnf" "${tmp}/bin/sudo" "${tmp}/bin/kbuildsycoca6"
export PATH="${tmp}/bin:${PATH}"
export INSTALL_TEST_CALLS="${tmp}/calls"
export CMAKE_BUILD_PARALLEL_LEVEL=2
# Keep legacy conversion away from the real home folder and /usr/local.
export HOME="${tmp}/home"
export XDG_CONFIG_HOME="${tmp}/home/.config"
export LEGACY_PREFIXES=""
export PLASMA_MAIL_OS_RELEASE="${tmp}/os-release"
# Keep the AppArmor step away from the host's kernel setting and /etc/apparmor.d.
export PLASMA_IMPRESARIO_USERNS_SYSCTL="${tmp}/userns-restricted"
export PLASMA_IMPRESARIO_APPARMOR_DIR="${tmp}/apparmor.d"
printf '0\n' > "${PLASMA_IMPRESARIO_USERNS_SYSCTL}"
mkdir "${PLASMA_IMPRESARIO_APPARMOR_DIR}"
printf 'ID=cachyos\nID_LIKE=arch\n' > "${PLASMA_MAIL_OS_RELEASE}"

"${root}/install.sh" --help > "${tmp}/output"
grep -Fq "Install and build Plasma Impresario locally." "${tmp}/output"
grep -Fq -- "--with-webengine" "${tmp}/output"
grep -Fq -- "--without-webengine" "${tmp}/output"
grep -Fq -- "--check-deps" "${tmp}/output"
grep -Fq -- "--build-only" "${tmp}/output"
grep -Fq -- "--keep-legacy" "${tmp}/output"
grep -Fq -- "--no-apparmor" "${tmp}/output"

: > "${INSTALL_TEST_CALLS}"
WITH_WEBENGINE=false "${root}/install.sh" --check-deps --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq "All installer-listed dependency packages are already installed" "${tmp}/output"
grep -Fq "CMake requirements were not verified" "${tmp}/output"
if grep -Eq '(^pacman -Syu |^-S |^--build |^--install |^sudo )' "${INSTALL_TEST_CALLS}"; then
  echo "Dependency check unexpectedly installed packages or built the app" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
if WITH_WEBENGINE=false MISSING_REQUIRED=qt6-svg "${root}/install.sh" --check-deps \
  --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Dependency check unexpectedly succeeded with missing packages" >&2
  exit 1
fi
grep -Fq "qt6-svg" "${tmp}/output"
if grep -Eq '(^pacman -Syu |^-S |^--build |^--install |^sudo )' "${INSTALL_TEST_CALLS}"; then
  echo "Dependency check unexpectedly installed packages or built the app" >&2
  exit 1
fi

for dependency in kconfig ki18n; do
  : > "${INSTALL_TEST_CALLS}"
  if MISSING_REQUIRED="${dependency}" "${root}/install.sh" --check-deps \
    --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
    echo "Dependency check unexpectedly succeeded without ${dependency}" >&2
    exit 1
  fi
  grep -Fq "${dependency}" "${tmp}/output"
done

: > "${INSTALL_TEST_CALLS}"
if "${root}/install.sh" --no-deps --check-deps > "${tmp}/output" 2>&1; then
  echo "Conflicting dependency options unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "cannot be combined" "${tmp}/output"
[[ ! -s "${INSTALL_TEST_CALLS}" ]]

: > "${INSTALL_TEST_CALLS}"
if "${root}/install.sh" --build-only --check-deps > "${tmp}/output" 2>&1; then
  echo "Conflicting operation modes unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "cannot be combined" "${tmp}/output"
[[ ! -s "${INSTALL_TEST_CALLS}" ]]

: > "${INSTALL_TEST_CALLS}"
WITH_WEBENGINE=false "${root}/install.sh" --no-deps --build-only \
  --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq -- "--build ${tmp}/build --parallel 2" "${INSTALL_TEST_CALLS}"
if grep -Eq '(^pacman |^--install |^sudo )' "${INSTALL_TEST_CALLS}"; then
  echo "Build-only mode unexpectedly checked dependencies or installed the app" >&2
  exit 1
fi
grep -Fq "Build complete; application was not installed" "${tmp}/output"

: > "${INSTALL_TEST_CALLS}"
WITH_WEBENGINE=false MISSING_REQUIRED=qt6-svg INSTALL_SUCCEEDS=true \
  "${root}/install.sh" --build-only --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq "pacman -Syu --noconfirm qt6-svg" "${INSTALL_TEST_CALLS}"
grep -Fq -- "--build ${tmp}/build --parallel 2" "${INSTALL_TEST_CALLS}"
if grep -Fq -- "--install ${tmp}/build" "${INSTALL_TEST_CALLS}"; then
  echo "Build-only mode installed the app after dependency setup" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
if MISSING_REQUIRED=qt6-svg PLASMA_IMPRESARIO_OS_RELEASE="${tmp}/os-release" PLASMA_MAIL_OS_RELEASE="${tmp}/missing" \
  "${root}/install.sh" --without-webengine --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Failed dependency installation unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "pacman -Qq" "${INSTALL_TEST_CALLS}"

: > "${INSTALL_TEST_CALLS}"
WITH_WEBENGINE=false "${root}/install.sh" --no-deps --build-dir "${tmp}/build" \
  --prefix "${tmp}/install path" > "${tmp}/output" 2>&1
grep -Fq -- "-DWITH_WEBENGINE=OFF" "${INSTALL_TEST_CALLS}"
grep -Fq -- "-DCMAKE_INSTALL_PREFIX=${tmp}/install path" "${INSTALL_TEST_CALLS}"
grep -Fq -- "--build ${tmp}/build --parallel 2" "${INSTALL_TEST_CALLS}"
grep -Fq -- "--install ${tmp}/build" "${INSTALL_TEST_CALLS}"
grep -Fq "Incomplete installation: message viewing is unavailable" "${tmp}/output"

: > "${INSTALL_TEST_CALLS}"
if WITH_WEBENGINE=invalid "${root}/install.sh" --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Invalid renderer unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "Invalid WITH_WEBENGINE value" "${tmp}/output"
[[ ! -s "${INSTALL_TEST_CALLS}" ]]

: > "${INSTALL_TEST_CALLS}"
if WITH_WEBENGINE=false MISSING_REQUIRED=qt6-svg "${root}/install.sh" --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Failed dependency installation unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "pacman -Syu --noconfirm qt6-svg" "${INSTALL_TEST_CALLS}"
if grep -Eq '^(-S |--build |--install )' "${INSTALL_TEST_CALLS}"; then
  echo "CMake ran after dependency installation failed" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
WITH_WEBENGINE=false "${root}/install.sh" --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq "Optional package 'mimetic' is not installed and could not be found" "${tmp}/output"
grep -Fq -- "--install ${tmp}/build" "${INSTALL_TEST_CALLS}"
if grep -Fq "pacman -Syu" "${INSTALL_TEST_CALLS}"; then
  echo "Unavailable optional package triggered package installation" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
if WITH_WEBENGINE=false OPTIONAL_AVAILABLE=true "${root}/install.sh" --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Failed optional package installation unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "pacman -Syu --noconfirm mimetic" "${INSTALL_TEST_CALLS}"
if grep -Eq '^(-S |--build |--install )' "${INSTALL_TEST_CALLS}"; then
  echo "CMake ran after optional package installation failed" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
if FAIL_CONFIGURE=true "${root}/install.sh" --no-deps --without-webengine \
  --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Failed configuration unexpectedly succeeded" >&2
  exit 1
fi
if grep -Eq '^--(build|install) ' "${INSTALL_TEST_CALLS}"; then
  echo "Build or installation ran after configuration failed" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
WITH_WEBENGINE=true MISSING_REQUIRED=qt6-webengine INSTALL_SUCCEEDS=true "${root}/install.sh" \
  --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq "pacman -Syu --noconfirm qt6-webengine" "${INSTALL_TEST_CALLS}"

: > "${INSTALL_TEST_CALLS}"
WITH_WEBENGINE=false MISSING_REQUIRED=qt6-webengine "${root}/install.sh" \
  --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
if grep -Fq "qt6-webengine" "${INSTALL_TEST_CALLS}"; then
  echo "Renderer-free build attempted to install Qt WebEngine" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
WITH_WEBENGINE=true "${root}/install.sh" --no-deps --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq -- "-DWITH_WEBENGINE=ON" "${INSTALL_TEST_CALLS}"
grep -Fq "Installation complete." "${tmp}/output"

: > "${INSTALL_TEST_CALLS}"
printf 'ID=example\nID_LIKE=fedora\n' > "${PLASMA_MAIL_OS_RELEASE}"
"${root}/install.sh" --without-webengine --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq "dnf list installed" "${INSTALL_TEST_CALLS}"
if grep -Fq "pacman " "${INSTALL_TEST_CALLS}"; then
  echo "Used the Arch package manager on a Fedora-derived system" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
printf 'ID=unsupported\n' > "${PLASMA_MAIL_OS_RELEASE}"
if "${root}/install.sh" --without-webengine --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Unsupported distribution unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "Unsupported distribution" "${tmp}/output"
[[ ! -s "${INSTALL_TEST_CALLS}" ]]

ln -s "$(command -v bash)" "${tmp}/limited/bash"
ln -s "$(command -v dirname)" "${tmp}/limited/dirname"
cat > "${tmp}/limited/paru" <<'EOF'
#!/usr/bin/env bash
printf 'paru %s\n' "$*" >> "${INSTALL_TEST_CALLS}"
exit 1
EOF
cat > "${tmp}/limited/sudo" <<'EOF'
#!/usr/bin/env bash
printf 'sudo %s\n' "$*" >> "${INSTALL_TEST_CALLS}"
exit 1
EOF
chmod +x "${tmp}/limited/paru" "${tmp}/limited/sudo"
printf 'ID=arch\n' > "${PLASMA_MAIL_OS_RELEASE}"
: > "${INSTALL_TEST_CALLS}"
if PATH="${tmp}/limited" "${root}/install.sh" --without-webengine --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Failed helper installation unexpectedly succeeded" >&2
  exit 1
fi
if [[ "${EUID}" -eq 0 ]]; then
  grep -Fq "paru must be run as a regular user" "${tmp}/output"
else
  grep -Fq "paru -Syu --noconfirm" "${INSTALL_TEST_CALLS}"
fi
if grep -Fq "sudo " "${INSTALL_TEST_CALLS}"; then
  echo "AUR helper was incorrectly run under sudo" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
printf 'ID=fedora\n' > "${PLASMA_MAIL_OS_RELEASE}"
if PATH="${tmp}/limited" "${root}/install.sh" --without-webengine --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Missing distribution package manager unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "No supported package manager found for 'fedora'" "${tmp}/output"
[[ ! -s "${INSTALL_TEST_CALLS}" ]]

# Legacy Trojita and Plasma Mail conversion.
make_legacy_prefix() {
  local prefix="$1"
  mkdir -p "${prefix}/bin" "${prefix}/lib/trojita" "${prefix}/share/applications" \
    "${prefix}/share/metainfo" "${prefix}/share/icons/hicolor/32x32/apps" \
    "${prefix}/share/icons/hicolor/scalable/apps" "${prefix}/share/man/man1" \
    "${prefix}/share/locale/de/LC_MESSAGES" "${prefix}/share/doc/HTML/en/trojita"
  printf '#!/bin/sh\n' > "${prefix}/bin/trojita"
  chmod +x "${prefix}/bin/trojita"
  touch "${prefix}/bin/be.contacts" "${prefix}/lib/libtrojita_plugins.so.1" \
    "${prefix}/lib/trojita/trojita_plugin_QtKeychainPasswordPlugin.so" \
    "${prefix}/share/applications/org.kde.trojita.desktop" \
    "${prefix}/share/applications/unrelated.desktop" \
    "${prefix}/share/metainfo/org.kde.trojita.appdata.xml" \
    "${prefix}/share/icons/hicolor/32x32/apps/trojita.png" \
    "${prefix}/share/icons/hicolor/scalable/apps/trojita.svg" \
    "${prefix}/share/man/man1/trojita.1" \
    "${prefix}/share/locale/de/LC_MESSAGES/trojita_qt.qm" \
    "${prefix}/share/doc/HTML/en/trojita/index.html"
}

write_legacy_mimeapps() {
  mkdir -p "${XDG_CONFIG_HOME}"
  cat > "${XDG_CONFIG_HOME}/mimeapps.list" <<'EOF'
# Chosen in Trojita: org.kde.trojita.desktop
[Default Applications]
x-scheme-handler/mailto=org.kde.trojita.desktop;io.github.mikepadiernos.plasma_impresario.desktop;
text/html=firefox.desktop;

[Added Associations]
message/rfc822=org.kde.trojita.desktop;kmail.desktop;
EOF
}

expect_legacy_unchanged() {
  local prefix="$1" context="$2"
  if [[ -L "${prefix}/bin/trojita" || ! -e "${prefix}/share/applications/org.kde.trojita.desktop" ]] \
    || ! grep -Fq "=org.kde.trojita.desktop;" "${XDG_CONFIG_HOME}/mimeapps.list"; then
    echo "${context} unexpectedly converted the legacy installation" >&2
    exit 1
  fi
}

printf 'ID=arch\n' > "${PLASMA_MAIL_OS_RELEASE}"
legacy="${tmp}/legacy"
current="${tmp}/current"
make_legacy_prefix "${legacy}"
make_legacy_prefix "${current}"
touch "${current}/bin/plasma-impresario"
ln -s /usr "${tmp}/usr-link"
mkdir -p "${tmp}/install/share/metainfo"
touch "${tmp}/install/share/metainfo/org.kde.trojita.appdata.xml"
write_legacy_mimeapps

: > "${INSTALL_TEST_CALLS}"
LEGACY_PREFIXES="${legacy}:${current}:${tmp}/usr-link:${tmp}/missing:${tmp}/install" \
  "${root}/install.sh" --no-deps --without-webengine --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1
[[ -L "${legacy}/bin/trojita" ]]
[[ "$(readlink "${legacy}/bin/trojita")" == "${tmp}/install/bin/plasma-impresario" ]]
for path in bin/be.contacts lib/libtrojita_plugins.so.1 lib/trojita \
  share/applications/org.kde.trojita.desktop share/metainfo/org.kde.trojita.appdata.xml \
  share/icons/hicolor/32x32/apps/trojita.png share/icons/hicolor/scalable/apps/trojita.svg \
  share/man/man1/trojita.1 share/locale/de/LC_MESSAGES/trojita_qt.qm share/doc/HTML/en/trojita; do
  if [[ -e "${legacy}/${path}" ]]; then
    echo "Legacy file ${path} was not removed" >&2
    exit 1
  fi
done
[[ -e "${legacy}/share/applications/unrelated.desktop" ]]
grep -Fq "Converting the Trojita installation in ${legacy}" "${tmp}/output"
if [[ -L "${current}/bin/trojita" || ! -e "${current}/share/applications/org.kde.trojita.desktop" ]]; then
  echo "A prefix with Plasma Impresario installed was treated as legacy" >&2
  exit 1
fi
grep -Fq "Skipping /usr: the package manager owns it." "${tmp}/output"
[[ ! -e "${tmp}/install/share/metainfo/org.kde.trojita.appdata.xml" ]]
cat > "${tmp}/expected-mimeapps" <<'EOF'
# Chosen in Trojita: org.kde.trojita.desktop
[Default Applications]
x-scheme-handler/mailto=io.github.mikepadiernos.plasma_impresario.desktop;
text/html=firefox.desktop;

[Added Associations]
message/rfc822=io.github.mikepadiernos.plasma_impresario.desktop;kmail.desktop;
EOF
if [[ "$(< "${tmp}/expected-mimeapps")" != "$(< "${XDG_CONFIG_HOME}/mimeapps.list")" ]]; then
  echo "Unexpected mail handler associations:" >&2
  cat "${XDG_CONFIG_HOME}/mimeapps.list" >&2
  exit 1
fi
grep -Fxq "kbuildsycoca6" "${INSTALL_TEST_CALLS}"

: > "${INSTALL_TEST_CALLS}"
LEGACY_PREFIXES="${legacy}" "${root}/install.sh" --no-deps --without-webengine \
  --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
if grep -Fq "Converting" "${tmp}/output" || grep -Fq "kbuildsycoca6" "${INSTALL_TEST_CALLS}"; then
  echo "A converted installation was converted again" >&2
  exit 1
fi

rm -rf -- "${legacy}"
make_legacy_prefix "${legacy}"
write_legacy_mimeapps
LEGACY_PREFIXES="${legacy}" "${root}/install.sh" --no-deps --without-webengine --keep-legacy \
  --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
expect_legacy_unchanged "${legacy}" "--keep-legacy"
CONVERT_LEGACY=false LEGACY_PREFIXES="${legacy}" "${root}/install.sh" --no-deps --without-webengine \
  --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
expect_legacy_unchanged "${legacy}" "CONVERT_LEGACY=false"
LEGACY_PREFIXES="${legacy}" "${root}/install.sh" --no-deps --without-webengine --build-only \
  --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
expect_legacy_unchanged "${legacy}" "--build-only"

# AppArmor profile for the renderer sandbox.
cat > "${tmp}/bin/apparmor_parser" <<'EOF'
#!/usr/bin/env bash
printf 'apparmor_parser %s\n' "$*" >> "${INSTALL_TEST_CALLS}"
EOF
chmod +x "${tmp}/bin/apparmor_parser"
profile="${PLASMA_IMPRESARIO_APPARMOR_DIR}/plasma-impresario"
mkdir -p "${tmp}/install/bin"

: > "${INSTALL_TEST_CALLS}"
"${root}/install.sh" --no-deps --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
if [[ -e "${profile}" ]] || grep -Fq "apparmor_parser" "${INSTALL_TEST_CALLS}"; then
  echo "An AppArmor profile was installed without the user namespace restriction" >&2
  exit 1
fi

printf '1\n' > "${PLASMA_IMPRESARIO_USERNS_SYSCTL}"
: > "${INSTALL_TEST_CALLS}"
"${root}/install.sh" --no-deps --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq "AppArmor profile installed: ${profile}" "${tmp}/output"
grep -Fxq "profile plasma-impresario \"$(cd -P "${tmp}/install/bin" && pwd)/plasma-impresario\" flags=(unconfined) {" "${profile}"
grep -Fxq "  userns," "${profile}"
if grep -Fq "@BINARY@" "${profile}"; then
  echo "The AppArmor profile still contains its placeholder" >&2
  exit 1
fi
grep -Fxq "apparmor_parser -r -- ${profile}" "${INSTALL_TEST_CALLS}"
grep -Fq "QtWebEngineProcess is missing" "${tmp}/output"

: > "${PLASMA_IMPRESARIO_APPARMOR_DIR}/QtWebEngineProcess"
mkdir -p "${tmp}/a&b/bin"
"${root}/install.sh" --no-deps --build-dir "${tmp}/build" --prefix "${tmp}/a&b" > "${tmp}/output" 2>&1
grep -Fq "/a&b/bin/plasma-impresario\" flags=(unconfined)" "${profile}"
if grep -Fq "QtWebEngineProcess is missing" "${tmp}/output"; then
  echo "Warned about a QtWebEngineProcess profile that exists" >&2
  exit 1
fi

for case in "--no-apparmor" "--without-webengine" "--build-only" "unsafe"; do
  rm -f -- "${profile}"
  : > "${INSTALL_TEST_CALLS}"
  prefix="${tmp}/install"
  option="${case}"
  if [[ "${case}" == "unsafe" ]]; then
    prefix="${tmp}/one,two"
    option="--no-deps"
  fi
  "${root}/install.sh" --no-deps "${option}" --build-dir "${tmp}/build" --prefix "${prefix}" > "${tmp}/output" 2>&1
  if [[ -e "${profile}" ]] || grep -Fq "apparmor_parser" "${INSTALL_TEST_CALLS}"; then
    echo "An AppArmor profile was installed with ${case}" >&2
    exit 1
  fi
done
grep -Fq "cannot write an AppArmor profile" "${tmp}/output"
"${root}/install.sh" --no-deps --no-apparmor --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq -- "--no-apparmor was given" "${tmp}/output"

rm "${tmp}/bin/apparmor_parser"
: > "${INSTALL_TEST_CALLS}"
PATH="${tmp}/bin:/usr/bin:/bin" "${root}/install.sh" --no-deps --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq "apparmor_parser is missing" "${tmp}/output"
[[ ! -e "${profile}" ]]
printf '0\n' > "${PLASMA_IMPRESARIO_USERNS_SYSCTL}"

echo "Installer checks passed."
