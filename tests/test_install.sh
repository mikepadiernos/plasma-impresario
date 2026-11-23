#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="$(mktemp -d)"
mkdir "${tmp}/bin"
trap 'rm -f "${tmp}/bin/cmake" "${tmp}/bin/pacman" "${tmp}/bin/sudo" "${tmp}/calls" "${tmp}/output"; rmdir "${tmp}/bin" "${tmp}"' EXIT

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
if [[ "${1:-}" == "-Qq" && "${2:-}" != "mimetic" ]]; then
  exit 0
fi
exit 1
EOF
cat > "${tmp}/bin/sudo" <<'EOF'
#!/usr/bin/env bash
exec "$@"
EOF
chmod +x "${tmp}/bin/cmake" "${tmp}/bin/pacman" "${tmp}/bin/sudo"
export PATH="${tmp}/bin:${PATH}"
export INSTALL_TEST_CALLS="${tmp}/calls"
export CMAKE_BUILD_PARALLEL_LEVEL=2

: > "${INSTALL_TEST_CALLS}"
WITH_WEBKIT=false "${root}/install.sh" --no-deps --build-dir "${tmp}/build" \
  --prefix "${tmp}/install path" > "${tmp}/output" 2>&1
grep -Fq -- "-DWITH_WEBKIT=OFF" "${INSTALL_TEST_CALLS}"
grep -Fq -- "-DCMAKE_INSTALL_PREFIX=${tmp}/install path" "${INSTALL_TEST_CALLS}"
grep -Fq -- "--build ${tmp}/build --parallel 2" "${INSTALL_TEST_CALLS}"
grep -Fq -- "--install ${tmp}/build" "${INSTALL_TEST_CALLS}"
grep -Fq "Incomplete installation: message viewing is unavailable" "${tmp}/output"

: > "${INSTALL_TEST_CALLS}"
if WITH_WEBKIT=invalid "${root}/install.sh" --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Invalid renderer unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "Invalid WITH_WEBKIT value" "${tmp}/output"
[[ ! -s "${INSTALL_TEST_CALLS}" ]]

: > "${INSTALL_TEST_CALLS}"
if WITH_WEBKIT=false "${root}/install.sh" --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Failed dependency installation unexpectedly succeeded" >&2
  exit 1
fi
grep -Fq "pacman -Syu --noconfirm mimetic" "${INSTALL_TEST_CALLS}"
if grep -Eq '^(-S |--build |--install )' "${INSTALL_TEST_CALLS}"; then
  echo "CMake ran after dependency installation failed" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
if FAIL_CONFIGURE=true "${root}/install.sh" --no-deps --without-webkit \
  --build-dir "${tmp}/build" --prefix "${tmp}/install" > "${tmp}/output" 2>&1; then
  echo "Failed configuration unexpectedly succeeded" >&2
  exit 1
fi
if grep -Eq '^--(build|install) ' "${INSTALL_TEST_CALLS}"; then
  echo "Build or installation ran after configuration failed" >&2
  exit 1
fi

: > "${INSTALL_TEST_CALLS}"
WITH_WEBKIT=true "${root}/install.sh" --no-deps --build-dir "${tmp}/build" \
  --prefix "${tmp}/install" > "${tmp}/output" 2>&1
grep -Fq -- "-DWITH_WEBKIT=ON" "${INSTALL_TEST_CALLS}"
grep -Fq "Installation complete." "${tmp}/output"

echo "Installer checks passed."
