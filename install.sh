#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Plasma Impresario"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
PREFIX="${PREFIX:-${HOME}/.local}"
INSTALL_DEPS="${INSTALL_DEPS:-true}"
CHECK_DEPS_ONLY=false
BUILD_ONLY=false
WITH_WEBENGINE="${WITH_WEBENGINE:-true}"
OS_RELEASE_FILE="${PLASMA_IMPRESARIO_OS_RELEASE:-${PLASMA_MAIL_OS_RELEASE:-/etc/os-release}}"
SUDO=""
PACKAGE_MANAGER=""

usage() {
  cat <<EOF
Usage: ./install.sh [options]

Install and build ${APP_NAME} locally.

Options:
  --no-deps           Skip dependency checks and package installation
  --check-deps        Check dependency packages only; do not install or build
  --build-only        Build without installing the app (dependencies may be installed)
  --prefix PATH       Install into PATH instead of \$HOME/.local
  --build-dir PATH    Use a custom build directory
  --with-webengine    Require Qt6 WebEngineWidgets (default; needed to read mail)
  --without-webengine Build without message viewing (incomplete mail client)
  --help              Show this help message
EOF
}

require_root_or_sudo() {
  if [[ "${EUID}" -eq 0 ]]; then
    SUDO=""
    return 0
  fi

  if command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
    return 0
  fi

  echo "[${APP_NAME}] Root privileges or sudo are required for this operation." >&2
  return 1
}

detect_package_manager() {
  local ID="" ID_LIKE="" distro candidate
  local -a candidates=() distros=()

  if [[ ! -r "${OS_RELEASE_FILE}" ]]; then
    echo "[${APP_NAME}] Cannot identify the distribution: ${OS_RELEASE_FILE} is not readable. Use --no-deps after installing dependencies manually." >&2
    return 1
  fi
  # os-release is specified as shell-compatible variable assignments.
  if ! source "${OS_RELEASE_FILE}"; then
    echo "[${APP_NAME}] Could not read distribution information from ${OS_RELEASE_FILE}." >&2
    return 1
  fi

  read -r -a distros <<< "${ID} ${ID_LIKE}"
  for distro in "${distros[@]}"; do
    case "${distro}" in
      arch|cachyos|manjaro) candidates=(pacman paru yay pamac) ;;
      debian|ubuntu) candidates=(apt-get) ;;
      fedora|rhel|centos|rocky|almalinux) candidates=(dnf) ;;
      opensuse*|suse|sles) candidates=(zypper) ;;
      *) continue ;;
    esac
    break
  done

  if [[ ${#candidates[@]} -eq 0 ]]; then
    echo "[${APP_NAME}] Unsupported distribution '${ID}'. Use --no-deps after installing dependencies manually." >&2
    return 1
  fi
  for candidate in "${candidates[@]}"; do
    if command -v "${candidate}" >/dev/null 2>&1; then
      PACKAGE_MANAGER="${candidate}"
      return 0
    fi
  done

  echo "[${APP_NAME}] No supported package manager found for '${ID}' (expected: ${candidates[*]}). Use --no-deps after installing dependencies manually." >&2
  return 1
}

package_installed() {
  local pkg="$1"

  case "${PACKAGE_MANAGER}" in
    pacman|paru|yay)
      "${PACKAGE_MANAGER}" -Qq "${pkg}" >/dev/null 2>&1
      ;;
    pamac)
      pamac list --installed --quiet | grep -Fxq "${pkg}"
      ;;
    apt-get)
      dpkg-query -W -f='${Status}' "${pkg}" 2>/dev/null | grep -q "install ok installed"
      ;;
    dnf)
      dnf list installed "${pkg}" >/dev/null 2>&1
      ;;
    zypper)
      zypper search --installed-only --quiet "${pkg}" >/dev/null 2>&1
      ;;
    *)
      return 1
      ;;
  esac
}

install_missing_packages() {
  local -a packages=("$@")
  local -a missing=()
  local pkg

  for pkg in "${packages[@]}"; do
    if ! package_installed "${pkg}"; then
      missing+=("${pkg}")
    fi
  done

  if [[ ${#missing[@]} -eq 0 ]]; then
    if [[ "${CHECK_DEPS_ONLY}" == "true" ]]; then
      echo "[${APP_NAME}] All installer-listed dependency packages are already installed."
    else
      echo "[${APP_NAME}] All build dependencies are already installed."
    fi
    return 0
  fi

  if [[ "${CHECK_DEPS_ONLY}" == "true" ]]; then
    echo "[${APP_NAME}] Missing dependency packages: ${missing[*]}" >&2
    return 1
  fi

  echo "[${APP_NAME}] Installing missing build dependencies: ${missing[*]}"
  case "${PACKAGE_MANAGER}" in
    pacman|apt-get|dnf|zypper) require_root_or_sudo ;;
    paru|yay)
      if [[ "${EUID}" -eq 0 ]]; then
        echo "[${APP_NAME}] ${PACKAGE_MANAGER} must be run as a regular user." >&2
        return 1
      fi
      ;;
  esac

  case "${PACKAGE_MANAGER}" in
    pacman)
      if [[ -n "${SUDO}" ]]; then
        ${SUDO} pacman -Syu --noconfirm "${missing[@]}"
      else
        pacman -Syu --noconfirm "${missing[@]}"
      fi
      ;;
    paru|yay)
      "${PACKAGE_MANAGER}" -Syu --noconfirm "${missing[@]}"
      ;;
    pamac)
      pamac install -y "${missing[@]}"
      ;;
    apt-get)
      if [[ -n "${SUDO}" ]]; then
        ${SUDO} apt-get update
        ${SUDO} env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${missing[@]}"
      else
        apt-get update
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${missing[@]}"
      fi
      ;;
    dnf)
      if [[ -n "${SUDO}" ]]; then
        ${SUDO} dnf install -y "${missing[@]}"
      else
        dnf install -y "${missing[@]}"
      fi
      ;;
    zypper)
      if [[ -n "${SUDO}" ]]; then
        ${SUDO} zypper install -y "${missing[@]}"
      else
        zypper install -y "${missing[@]}"
      fi
      ;;
    *)
      echo "[${APP_NAME}] Unsupported package manager '${PACKAGE_MANAGER}'." >&2
      return 1
      ;;
  esac
}

install_dependencies() {
  if [[ "${INSTALL_DEPS}" != "true" ]]; then
    return 0
  fi

  if ! detect_package_manager; then
    return 1
  fi

  local -a webengine_packages=()
  if [[ "${WITH_WEBENGINE}" != "false" ]]; then
    case "${PACKAGE_MANAGER}" in
      pacman|paru|yay|pamac) webengine_packages=(qt6-webengine) ;;
      apt-get) webengine_packages=(qt6-webengine-dev) ;;
      dnf) webengine_packages=(qt6-qtwebengine-devel) ;;
      zypper) webengine_packages=(qt6-webenginewidgets-devel) ;;
    esac
  fi

  case "${PACKAGE_MANAGER}" in
    pacman)
      local pkg
      local -a available_optional=()
      local -a optional_packages=(mimetic gpgme gpgmepp qgpgme qtkeychain-qt6 kdoctools sonnet akonadi akonadi-contacts)
      for pkg in "${optional_packages[@]}"; do
        if package_installed "${pkg}" || pacman -Si "${pkg}" >/dev/null 2>&1; then
          available_optional+=("${pkg}")
        else
          echo "[${APP_NAME}] Optional package '${pkg}' is not installed and could not be found in configured repositories; check repository metadata or install it manually if needed." >&2
        fi
      done
      install_missing_packages \
        base-devel cmake ninja pkgconf extra-cmake-modules qt6-base \
        qt6-declarative qt6-svg qt6-tools qt6-5compat kitemmodels \
        "${webengine_packages[@]}" "${available_optional[@]}"
      return 0
      ;;
    paru|yay|pamac)
      install_missing_packages \
        base-devel \
        cmake \
        ninja \
        pkgconf \
        extra-cmake-modules \
        qt6-base \
        qt6-declarative \
        qt6-svg \
        qt6-tools \
        qt6-5compat \
        kitemmodels \
        "${webengine_packages[@]}" \
        mimetic \
        gpgme \
        gpgmepp \
        qgpgme \
        qtkeychain-qt6 \
        kdoctools \
        sonnet \
        akonadi \
        akonadi-contacts
      return 0
      ;;
    apt-get)
      install_missing_packages \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        extra-cmake-modules \
        qt6-base-dev \
        qt6-declarative-dev \
        qt6-base-private-dev \
        qt6-tools-dev \
        qt6-tools-dev-tools \
        qt6-svg-dev \
        qt6-5compat-dev \
        "${webengine_packages[@]}" \
        libqt6sql6-dev \
        libqt6dbus6-dev \
        libkf6itemmodels-dev \
        libkf6sonnet-dev \
        libkf6akonadicore-dev \
        libkf6akonadimime-dev
      return 0
      ;;
    dnf)
      install_missing_packages \
        gcc-c++ \
        cmake \
        ninja-build \
        make \
        pkgconf-pkg-config \
        extra-cmake-modules \
        qt6-qtbase-devel \
        qt6-qtdeclarative-devel \
        qt6-qtsvg-devel \
        "${webengine_packages[@]}" \
        qt6-qttools-devel \
        qt6-qt5compat-devel \
        kf6-kitemmodels-devel \
        kf6-sonnet-devel \
        kf6-akonadi-devel
      return 0
      ;;
    zypper)
      install_missing_packages \
        gcc-c++ \
        cmake \
        ninja \
        pkg-config \
        extra-cmake-modules \
        libqt6-qtbase-common-devel \
        libqt6-qtdeclarative-devel \
        libQt6Svg6-devel \
        "${webengine_packages[@]}" \
        qt6-tools-devel \
        libqt6-qt5compat-devel \
        kitemmodels-devel \
        sonnet-devel \
        akonadi-devel
      return 0
      ;;
  esac

  echo "[${APP_NAME}] Unsupported package manager '${PACKAGE_MANAGER}'." >&2
  return 1
}

select_renderer() {
  case "${WITH_WEBENGINE}" in
    true|auto)
      WEBENGINE_FLAG="-DWITH_WEBENGINE=ON"
      echo "[${APP_NAME}] Message viewing requires Qt6 WebEngineWidgets."
      ;;
    false)
      WEBENGINE_FLAG="-DWITH_WEBENGINE=OFF"
      echo "[${APP_NAME}] Warning: this build cannot display message contents." >&2
      ;;
    *) echo "[${APP_NAME}] Invalid WITH_WEBENGINE value: ${WITH_WEBENGINE}" >&2; return 1 ;;
  esac
}

configure_and_build() {
  echo "[${APP_NAME}] Configuring project with ${WEBENGINE_FLAG}..."
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -G Ninja \
    -DQT_MAJOR_VERSION=6 \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_INSTALL_RPATH="${PREFIX}/lib" \
    "${WEBENGINE_FLAG}"

  echo "[${APP_NAME}] Building project..."
  cmake --build "${BUILD_DIR}" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc 2>/dev/null || echo 2)}"

  if [[ "${BUILD_ONLY}" == "true" ]]; then
    return 0
  fi

  echo "[${APP_NAME}] Installing project..."
  local install_parent="${PREFIX}"
  while [[ ! -e "${install_parent}" ]]; do
    install_parent="$(dirname "${install_parent}")"
  done
  if [[ -w "${install_parent}" || "${EUID}" -eq 0 ]]; then
    cmake --install "${BUILD_DIR}"
  else
    require_root_or_sudo
    echo "[${APP_NAME}] Installing to ${PREFIX} requires administrator privileges."
    "${SUDO}" cmake --install "${BUILD_DIR}"
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-deps)
      INSTALL_DEPS=false
      shift
      ;;
    --check-deps)
      CHECK_DEPS_ONLY=true
      shift
      ;;
    --build-only)
      BUILD_ONLY=true
      shift
      ;;
    --prefix)
      [[ $# -ge 2 && -n "$2" ]] || { echo "--prefix requires a path" >&2; exit 1; }
      PREFIX="$2"
      shift 2
      ;;
    --build-dir)
      [[ $# -ge 2 && -n "$2" ]] || { echo "--build-dir requires a path" >&2; exit 1; }
      BUILD_DIR="$2"
      shift 2
      ;;
    --with-webengine)
      WITH_WEBENGINE="true"
      shift
      ;;
    --without-webengine)
      WITH_WEBENGINE="false"
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ "${CHECK_DEPS_ONLY}" == "true" && ( "${INSTALL_DEPS}" != "true" || "${BUILD_ONLY}" == "true" ) ]]; then
  echo "[${APP_NAME}] --check-deps cannot be combined with --no-deps or --build-only." >&2
  exit 1
fi

if [[ "${PREFIX}" != /* ]]; then
  echo "[${APP_NAME}] The installation prefix must be an absolute path." >&2
  exit 1
fi

echo "[${APP_NAME}] Preparing installation for ${ROOT_DIR}"
select_renderer
install_dependencies
if [[ "${CHECK_DEPS_ONLY}" == "true" ]]; then
  echo "[${APP_NAME}] Package check complete; CMake requirements were not verified."
  exit 0
fi
configure_and_build

if [[ "${BUILD_ONLY}" == "true" ]]; then
  echo "[${APP_NAME}] Build complete; application was not installed."
elif [[ "${WITH_WEBENGINE}" == "false" ]]; then
  echo "[${APP_NAME}] Incomplete installation: message viewing is unavailable." >&2
else
  echo "[${APP_NAME}] Installation complete."
fi
echo "[${APP_NAME}] Build directory: ${BUILD_DIR}"
if [[ "${BUILD_ONLY}" != "true" ]]; then
  echo "[${APP_NAME}] Installed prefix: ${PREFIX}"
fi
