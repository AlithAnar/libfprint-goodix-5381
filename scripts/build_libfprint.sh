#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_WORKSPACE="${REPO_ROOT}/build-upstream"
LIBFPRINT_DIR="${BUILD_WORKSPACE}/libfprint"
UPSTREAM_URL="https://gitlab.freedesktop.org/libfprint/libfprint.git"

echo "==> Preparing upstream libfprint workspace in ${BUILD_WORKSPACE}"
mkdir -p "${BUILD_WORKSPACE}"

if [ ! -d "${LIBFPRINT_DIR}" ]; then
    git clone --depth 1 "${UPSTREAM_URL}" "${LIBFPRINT_DIR}"
else
    git -C "${LIBFPRINT_DIR}" checkout -- .
    git -C "${LIBFPRINT_DIR}" pull --ff-only
fi

echo "==> Copying goodix5381 driver sources"
cp "${REPO_ROOT}/driver/goodix5381.c" "${LIBFPRINT_DIR}/libfprint/drivers/"
cp "${REPO_ROOT}/driver/goodix5381.h" "${LIBFPRINT_DIR}/libfprint/drivers/"

echo "==> Patching meson.build files"
python3 - <<EOF
from pathlib import Path

root_meson = Path("${LIBFPRINT_DIR}/meson.build")
txt = root_meson.read_text()
if "'goodix5381'" not in txt:
    txt = txt.replace("all_drivers = [\n", "all_drivers = [\n    'goodix5381',\n", 1)
    txt = txt.replace("default_drivers = [\n", "default_drivers = [\n    'goodix5381',\n", 1)
    root_meson.write_text(txt)

lib_meson = Path("${LIBFPRINT_DIR}/libfprint/meson.build")
ltxt = lib_meson.read_text()
if "'goodix5381'" not in ltxt:
    ltxt = ltxt.replace("driver_sources = {\n", "driver_sources = {\n    'goodix5381' : [ 'drivers/goodix5381.c' ],\n", 1)
    lib_meson.write_text(ltxt)
EOF

echo "==> Configuring and compiling with Meson & Ninja"
cd "${LIBFPRINT_DIR}"
meson setup builddir --wipe -Ddrivers=goodix5381 -Ddoc=false -Dgtk-examples=false
ninja -C builddir

echo ""
echo "Build succeeded! To install system-wide, run:"
echo "  sudo ninja -C ${LIBFPRINT_DIR}/builddir install"
