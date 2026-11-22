#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Plasma Mail"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
PREFIX="${PREFIX:-/usr/local}"
INSTALL_DEPS="${INSTALL_DEPS:-true}"
SUDO=""
PACKAGE_MANAGER=""

usage() {
  cat <<EOF
Usage: ./install.sh [options]

Install and build ${APP_NAME} locally.

Options:
  --no-deps           Skip installing system dependencies
  --prefix PATH       Install into PATH instead of /usr/local
  --build-dir PATH    Use a custom build directory
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

  echo "Warning: neither root nor sudo is available; continuing without package installation." >&2
  SUDO=""
}

detect_package_manager() {
  local candidate
  for candidate in pacman paru yay pamac apt-get dnf zypper; do
    if command -v "${candidate}" >/dev/null 2>&1; then
      case "${candidate}" in
        pacman)
          PACKAGE_MANAGER="pacman"
          ;;
        paru)
          PACKAGE_MANAGER="paru"
          ;;
        yay)
          PACKAGE_MANAGER="yay"
          ;;
        pamac)
          PACKAGE_MANAGER="pamac"
          ;;
        apt-get)
          PACKAGE_MANAGER="apt-get"
          ;;
        dnf)
          PACKAGE_MANAGER="dnf"
          ;;
        zypper)
          PACKAGE_MANAGER="zypper"
          ;;
      esac
      return 0
    fi
  done

  return 1
}

install_dependencies() {
  if [[ "${INSTALL_DEPS}" != "true" ]]; then
    return 0
  fi

  if ! detect_package_manager; then
    echo "[${APP_NAME}] No supported package manager found. Please install CMake, Qt 6, KDE Frameworks, and build tools manually."
    return 0
  fi

  require_root_or_sudo

  case "${PACKAGE_MANAGER}" in
    pacman)
      echo "[${APP_NAME}] Installing Arch build dependencies with pacman..."
      if [[ -n "${SUDO}" ]]; then
        ${SUDO} pacman -Syu --noconfirm \
          base-devel \
          cmake \
          ninja \
          pkgconf \
          extra-cmake-modules \
          qt6-base \
          qt6-svg \
          qt6-tools \
          qt6-5compat \
          qt6-webengine \
          kitemmodels \
          sonnet
      else
        pacman -Syu --noconfirm \
          base-devel \
          cmake \
          ninja \
          pkgconf \
          extra-cmake-modules \
          qt6-base \
          qt6-svg \
          qt6-tools \
          qt6-5compat \
          qt6-webengine \
          kitemmodels \
          sonnet
      fi
      return 0
      ;;
    paru|yay)
      echo "[${APP_NAME}] Installing Arch build dependencies with ${PACKAGE_MANAGER}..."
      if [[ -n "${SUDO}" ]]; then
        ${SUDO} "${PACKAGE_MANAGER}" -Syu --noconfirm \
          base-devel \
          cmake \
          ninja \
          pkgconf \
          extra-cmake-modules \
          qt6-base \
          qt6-svg \
          qt6-tools \
          qt6-5compat \
          qt6-webengine \
          kitemmodels \
          sonnet
      else
        "${PACKAGE_MANAGER}" -Syu --noconfirm \
          base-devel \
          cmake \
          ninja \
          pkgconf \
          extra-cmake-modules \
          qt6-base \
          qt6-svg \
          qt6-tools \
          qt6-5compat \
          qt6-webengine \
          kitemmodels \
          sonnet
      fi
      return 0
      ;;
    pamac)
      echo "[${APP_NAME}] Installing Arch build dependencies with pamac..."
      if [[ -n "${SUDO}" ]]; then
        ${SUDO} pamac install -y \
          base-devel \
          cmake \
          ninja \
          pkgconf \
          extra-cmake-modules \
          qt6-base \
          qt6-svg \
          qt6-tools \
          qt6-5compat \
          qt6-webengine \
          kitemmodels \
          sonnet
      else
        pamac install -y \
          base-devel \
          cmake \
          ninja \
          pkgconf \
          extra-cmake-modules \
          qt6-base \
          qt6-svg \
          qt6-tools \
          qt6-5compat \
          qt6-webengine \
          kitemmodels \
          sonnet
      fi
      return 0
      ;;
    apt-get)
      echo "[${APP_NAME}] Installing Debian/Ubuntu build dependencies..."
      if [[ -n "${SUDO}" ]]; then
        ${SUDO} apt-get update
        ${SUDO} DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
          build-essential \
          cmake \
          ninja-build \
          pkg-config \
          extra-cmake-modules \
          qt6-base-dev \
          qt6-base-private-dev \
          qt6-tools-dev \
          qt6-tools-dev-tools \
          qt6-svg-dev \
          qt6-5compat-dev \
          libqt6sql6-dev \
          libqt6dbus6-dev \
          libkf6itemmodels-dev \
          libkf6sonnet-dev \
          qt6-webengine-dev
      else
        apt-get update
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
          build-essential \
          cmake \
          ninja-build \
          pkg-config \
          extra-cmake-modules \
          qt6-base-dev \
          qt6-base-private-dev \
          qt6-tools-dev \
          qt6-tools-dev-tools \
          qt6-svg-dev \
          qt6-5compat-dev \
          libqt6sql6-dev \
          libqt6dbus6-dev \
          libkf6itemmodels-dev \
          libkf6sonnet-dev \
          qt6-webengine-dev
      fi
      return 0
      ;;
    dnf)
      echo "[${APP_NAME}] Installing Fedora/RHEL build dependencies..."
      if [[ -n "${SUDO}" ]]; then
        ${SUDO} dnf install -y \
          gcc-c++ \
          cmake \
          ninja-build \
          make \
          pkgconf-pkg-config \
          extra-cmake-modules \
          qt6-qtbase-devel \
          qt6-qtsvg-devel \
          qt6-qttools-devel \
          qt6-qt5compat-devel \
          qt6-qtwebengine-devel \
          kf6-kitemmodels-devel \
          kf6-sonnet-devel
      else
        dnf install -y \
          gcc-c++ \
          cmake \
          ninja-build \
          make \
          pkgconf-pkg-config \
          extra-cmake-modules \
          qt6-qtbase-devel \
          qt6-qtsvg-devel \
          qt6-qttools-devel \
          qt6-qt5compat-devel \
          qt6-qtwebengine-devel \
          kf6-kitemmodels-devel \
          kf6-sonnet-devel
      fi
      return 0
      ;;
    zypper)
      echo "[${APP_NAME}] Installing openSUSE build dependencies..."
      if [[ -n "${SUDO}" ]]; then
        ${SUDO} zypper install -y \
          gcc-c++ \
          cmake \
          ninja \
          pkg-config \
          extra-cmake-modules \
          libqt6-qtbase-common-devel \
          libQt6Svg6-devel \
          qt6-tools-devel \
          libqt6-qt5compat-devel \
          libqt6-qtwebengine-devel \
          kitemmodels-devel \
          sonnet-devel
      else
        zypper install -y \
          gcc-c++ \
          cmake \
          ninja \
          pkg-config \
          extra-cmake-modules \
          libqt6-qtbase-common-devel \
          libQt6Svg6-devel \
          qt6-tools-devel \
          libqt6-qt5compat-devel \
          libqt6-qtwebengine-devel \
          kitemmodels-devel \
          sonnet-devel
      fi
      return 0
      ;;
  esac

  echo "[${APP_NAME}] No supported package manager found. Please install CMake, Qt 6, KDE Frameworks, and build tools manually."
  return 0
}

configure_and_build() {
  echo "[${APP_NAME}] Configuring project..."
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}"

  echo "[${APP_NAME}] Building project..."
  cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 2)"

  echo "[${APP_NAME}] Installing project..."
  cmake --install "${BUILD_DIR}" --prefix "${PREFIX}"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-deps)
      INSTALL_DEPS=false
      shift
      ;;
    --prefix)
      PREFIX="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
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

echo "[${APP_NAME}] Preparing installation for ${ROOT_DIR}"
install_dependencies
configure_and_build

echo "[${APP_NAME}] Installation complete."
echo "[${APP_NAME}] Build directory: ${BUILD_DIR}"
echo "[${APP_NAME}] Installed prefix: ${PREFIX}"
