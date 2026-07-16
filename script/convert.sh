#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 2 ]]; then
  echo "usage: ./script/convert.sh <input_csv> <output_belz>" >&2
  exit 2
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/engine-release}"
CONVERTER="${BUILD_DIR}/clickbench_convert"
INPUT_CSV="$1"
COLUMNAR="$2"
LOG_FILE="${COLUMNAR}.convert.log"

mkdir -p "$(dirname "${COLUMNAR}")"
mkdir -p "$(dirname "${LOG_FILE}")"

if [[ ! -x "${CONVERTER}" ]]; then
  "${ROOT_DIR}/script/build.sh"
fi

start_ms="$(date +%s%3N)"
{
  printf 'phase=convert\n'
  printf 'input_csv=%s\n' "${INPUT_CSV}"
  printf 'columnar=%s\n' "${COLUMNAR}"
  printf 'converter=%s\n' "${CONVERTER}"
  printf 'start_epoch_ms=%s\n' "${start_ms}"
} > "${LOG_FILE}"

tmp_stdout="$(mktemp)"
tmp_stderr="$(mktemp)"
if "${CONVERTER}" "${INPUT_CSV}" "${COLUMNAR}" >"${tmp_stdout}" 2>"${tmp_stderr}"; then
  status="ok"
  exit_code=0
else
  status="error"
  exit_code=$?
fi

finish_ms="$(date +%s%3N)"
{
  printf 'status=%s\n' "${status}"
  printf 'exit_code=%s\n' "${exit_code}"
  printf 'finish_epoch_ms=%s\n' "${finish_ms}"
  printf 'wall_ms=%s\n' "$((finish_ms - start_ms))"
  if [[ -f "${INPUT_CSV}" ]]; then
    printf 'input_bytes=%s\n' "$(stat -c '%s' "${INPUT_CSV}" 2>/dev/null || echo 0)"
  fi
  if [[ -f "${COLUMNAR}" ]]; then
    printf 'output_bytes=%s\n' "$(stat -c '%s' "${COLUMNAR}" 2>/dev/null || echo 0)"
  fi
  printf '\n[converter_stdout]\n'
  cat "${tmp_stdout}"
  printf '\n[converter_stderr]\n'
  cat "${tmp_stderr}"
} >> "${LOG_FILE}"

cat "${tmp_stdout}"
cat "${tmp_stderr}" >&2
rm -f "${tmp_stdout}" "${tmp_stderr}"

exit "${exit_code}"
