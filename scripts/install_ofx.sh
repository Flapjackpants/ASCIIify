#!/usr/bin/env bash
set -euo pipefail

DEFAULT_SDK="/Library/Application Support/Blackmagic Design/DaVinci Resolve/Developer/OpenFX"
DEFAULT_PLUGINS_DIR="/Library/OFX/Plugins"

usage() {
  cat <<EOF
Usage: $(basename "$0") [SDK_PATH] [--dest PLUGINS_DIR] [-h|--help]

Install ASCIIify.ofx.bundle for DaVinci Resolve.

Arguments:
  SDK_PATH            Path to the DaVinci Resolve OpenFX SDK (build-time headers).
                      Default: ${DEFAULT_SDK}

Options:
  --dest PLUGINS_DIR  OFX plugins directory (runtime install location).
                      Default: ${DEFAULT_PLUGINS_DIR}
  -h, --help          Show this help

Examples:
  $(basename "$0")
  $(basename "$0") "${DEFAULT_SDK}"
  $(basename "$0") --dest "\$HOME/Library/OFX/Plugins"
  $(basename "$0") "/path/to/OpenFX" --dest "\$HOME/Library/OFX/Plugins"

Note: The SDK path is where Resolve developer headers live. Plugins are installed
into PLUGINS_DIR, not the SDK folder.
EOF
}

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUNDLE="${ROOT}/build/ASCIIify.ofx.bundle"

SDK_PATH="${DEFAULT_SDK}"
PLUGINS_DIR="${DEFAULT_PLUGINS_DIR}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --dest)
      if [[ $# -lt 2 ]]; then
        echo "Error: --dest requires a path argument" >&2
        exit 1
      fi
      PLUGINS_DIR="$2"
      shift 2
      ;;
    --)
      shift
      break
      ;;
    -*)
      echo "Error: unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
    *)
      SDK_PATH="$1"
      shift
      ;;
  esac
done

SDK_HEADER="${SDK_PATH}/Support/include/ofxsImageEffect.h"
if [[ ! -f "${SDK_HEADER}" ]]; then
  echo "Error: OpenFX SDK not found at: ${SDK_PATH}" >&2
  echo "Expected header: ${SDK_HEADER}" >&2
  echo "Install DaVinci Resolve or pass the correct SDK path." >&2
  exit 1
fi

if [[ ! -d "${BUNDLE}" ]]; then
  echo "Error: bundle not found at ${BUNDLE}" >&2
  echo "Build first:" >&2
  echo "  mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build ." >&2
  exit 1
fi

# Expand ~ in plugins dir
PLUGINS_DIR="${PLUGINS_DIR/#\~/$HOME}"
DEST="${PLUGINS_DIR}/ASCIIify.ofx.bundle"

# Use sudo only for system-wide paths outside the user's home directory
USE_SUDO=0
case "${PLUGINS_DIR}" in
  "${HOME}"/*) USE_SUDO=0 ;;
  *) USE_SUDO=1 ;;
esac

run_privileged() {
  if [[ "${USE_SUDO}" -eq 1 ]]; then
    sudo "$@"
  else
    "$@"
  fi
}

echo "OpenFX SDK : ${SDK_PATH}"
echo "Installing : ${BUNDLE}"
echo "         -> ${DEST}"
if [[ "${USE_SUDO}" -eq 1 ]]; then
  echo "(using sudo for system install path)"
fi

run_privileged mkdir -p "${PLUGINS_DIR}"
run_privileged rm -rf "${DEST}"
run_privileged cp -R "${BUNDLE}" "${DEST}"
run_privileged xattr -cr "${DEST}"

echo ""
echo "Done. Next steps:"
echo "  1. Open DaVinci Resolve -> Preferences -> Video Plugins"
echo "  2. Enable ASCIIify, restart Resolve"
echo "  3. Edit/Color page -> FX -> OpenFX -> ASCIIify"
