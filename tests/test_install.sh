#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="$(mktemp -d)"
mkdir "${tmp}/bin" "${tmp}/limited"
trap 'rm -f "${tmp}/bin/cmake" "${tmp}/bin/pacman" "${tmp}/bin/dnf" "${tmp}/bin/sudo" "${tmp}/limited/bash" "${tmp}/limited/dirname" "${tmp}/limited/paru" "${tmp}/limited/sudo" "${tmp}/calls" "${tmp}/output" "${tmp}/os-release"; rmdir "${tmp}/bin" "${tmp}/limited" "${tmp}"' EXIT

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
chmod +x "${tmp}/bin/cmake" "${tmp}/bin/pacman" "${tmp}/bin/dnf" "${tmp}/bin/sudo"
export PATH="${tmp}/bin:${PATH}"
export INSTALL_TEST_CALLS="${tmp}/calls"
export CMAKE_BUILD_PARALLEL_LEVEL=2
export PLASMA_MAIL_OS_RELEASE="${tmp}/os-release"
printf 'ID=cachyos\nID_LIKE=arch\n' > "${PLASMA_MAIL_OS_RELEASE}"

"${root}/install.sh" --help > "${tmp}/output"
grep -Fq "Install and build Plasma Impresario locally." "${tmp}/output"
grep -Fq -- "--with-webengine" "${tmp}/output"
grep -Fq -- "--without-webengine" "${tmp}/output"
grep -Fq -- "--check-deps" "${tmp}/output"
grep -Fq -- "--build-only" "${tmp}/output"

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

echo "Installer checks passed."
