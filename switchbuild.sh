#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  printf 'Usage: %s [build-directory]\n' "$0"
  printf '\nBuilds the DuckStation Nintendo Switch frontend and emits duckstation.nro.\n'
  exit 0
fi

DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
DEVKITA64="${DEVKITA64:-${DEVKITPRO}/devkitA64}"
UAM_PREFIX="${UAM_PREFIX:-${DEVKITPRO}/portlibs/switch}"
BUILD_DIR="${1:-${PROJECT_DIR}/build-switch}"

if [[ "${BUILD_DIR}" != /* ]]; then
  BUILD_DIR="${PROJECT_DIR}/${BUILD_DIR}"
fi

if [[ "$#" -gt 1 ]]; then
  printf 'Usage: %s [build-directory]\n' "$0" >&2
  exit 2
fi

# CMake cannot change generators in an existing build tree. Keep the old tree
# intact and select a sibling directory when the requested tree was configured
# with a generator other than Ninja.
if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
  EXISTING_GENERATOR="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "${BUILD_DIR}/CMakeCache.txt" | head -n 1 || true)"
  if [[ -n "${EXISTING_GENERATOR}" && "${EXISTING_GENERATOR}" != "Ninja" ]]; then
    ORIGINAL_BUILD_DIR="${BUILD_DIR}"
    BUILD_DIR="${BUILD_DIR}-ninja"
    printf 'Notice: %s uses %s; using fresh Ninja directory %s\n' \
      "${ORIGINAL_BUILD_DIR}" "${EXISTING_GENERATOR}" "${BUILD_DIR}"
  fi
fi

CMAKE="${DEVKITPRO}/portlibs/switch/bin/aarch64-none-elf-cmake"
NINJA="$(command -v ninja || true)"
UAM_HEADER="${UAM_PREFIX}/include/uam.h"
UAM_LIBRARY="${UAM_PREFIX}/lib/libuam.a"

if [[ ! -x "${CMAKE}" ]]; then
  printf 'error: Switch CMake toolchain not found: %s\n' "${CMAKE}" >&2
  exit 1
fi

if [[ -z "${NINJA}" ]]; then
  printf 'error: Ninja not found in PATH\n' >&2
  exit 1
fi

if [[ ! -f "${UAM_HEADER}" || ! -f "${UAM_LIBRARY}" ]]; then
  printf 'error: UAM installation not found under %s\n' "${UAM_PREFIX}" >&2
  printf '       expected %s and %s\n' "${UAM_HEADER}" "${UAM_LIBRARY}" >&2
  exit 1
fi

export DEVKITPRO DEVKITA64
export PATH="${DEVKITPRO}/tools/bin:${DEVKITA64}/bin:${DEVKITPRO}/libnx/bin:${PATH}"

JOB_COUNT="${JOBS:-}"
if [[ -z "${JOB_COUNT}" ]] && command -v sysctl >/dev/null 2>&1; then
  JOB_COUNT="$(sysctl -n hw.ncpu 2>/dev/null || true)"
fi
if [[ -z "${JOB_COUNT}" ]] && command -v getconf >/dev/null 2>&1; then
  JOB_COUNT="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
fi
if [[ -z "${JOB_COUNT}" ]]; then
  JOB_COUNT=4
fi

printf 'Configuring DuckStation for Nintendo Switch...\n'
"${CMAKE}" \
  -S "${PROJECT_DIR}" \
  -B "${BUILD_DIR}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_NOGUI_FRONTEND=ON \
  -DBUILD_QT_FRONTEND=OFF \
  -DENABLE_OPENGL=OFF \
  -DENABLE_VULKAN=OFF \
  -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON \
  -DCMAKE_C_FLAGS="-D__SWITCH__" \
  -DCMAKE_CXX_FLAGS="-D__SWITCH__ -I${UAM_PREFIX}/include" \
  -DCMAKE_EXE_LINKER_FLAGS="-L${UAM_PREFIX}/lib -L${DEVKITPRO}/libnx/lib -L${DEVKITPRO}/portlibs/switch/lib -specs=${DEVKITPRO}/libnx/switch.specs -fPIE"

printf 'Building NRO with %s job(s)...\n' "${JOB_COUNT}"
"${CMAKE}" --build "${BUILD_DIR}" --parallel "${JOB_COUNT}"

NRO="${BUILD_DIR}/duckstation.nro"
if [[ ! -f "${NRO}" ]]; then
  printf 'error: build completed but NRO was not produced: %s\n' "${NRO}" >&2
  exit 1
fi

printf '\nNRO: %s\n' "${NRO}"
printf 'Size: %s bytes\n' "$(wc -c < "${NRO}" | tr -d '[:space:]')"
if command -v shasum >/dev/null 2>&1; then
  printf 'SHA-256: '
  shasum -a 256 "${NRO}"
else
  printf 'SHA-256: '
  sha256sum "${NRO}"
fi
