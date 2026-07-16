#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/engine-release}"
BUILD_JOBS="${BUILD_JOBS:-$(nproc 2>/dev/null || echo 1)}"

start_ms="$(date +%s%3N)"
mkdir -p "${BUILD_DIR}"

printf '[build] root=%s\n' "${ROOT_DIR}"
printf '[build] build_dir=%s\n' "${BUILD_DIR}"
printf '[build] jobs=%s\n' "${BUILD_JOBS}"

cmake -S "${ROOT_DIR}/Engine" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --target clickbench_runner clickbench_convert -j"${BUILD_JOBS}"

finish_ms="$(date +%s%3N)"
printf '[build] total_ms=%s\n' "$((finish_ms - start_ms))"
