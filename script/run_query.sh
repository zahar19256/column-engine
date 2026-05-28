#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 4 ]]; then
  echo "usage: ./script/run_query.sh <zero_based_query_num> <columnar_belz> <output_csv> <log_file>" >&2
  exit 2
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/engine-release}"
RUNNER="${BUILD_DIR}/clickbench_runner"
QUERY_INDEX="$1"
COLUMNAR="$2"
OUTPUT="$3"
LOG_FILE="$4"
CLICKBENCH_QUERY=$((QUERY_INDEX + 1))
RESULTS_DIR="$(dirname "${OUTPUT}")"
GLOBAL_LOG="${RESULTS_DIR}/summary.csv"

mkdir -p "${RESULTS_DIR}"
mkdir -p "$(dirname "${LOG_FILE}")"

if [[ ! -x "${RUNNER}" ]]; then
  "${ROOT_DIR}/script/build.sh"
fi

tmp_dir="$(mktemp -d)"
tmp_stdout="${tmp_dir}/runner.stdout"
tmp_stderr="${tmp_dir}/runner.stderr"
summary="${tmp_dir}/summary.csv"
runner_output="${tmp_dir}/q${CLICKBENCH_QUERY}.csv"
start_ms="$(date +%s%3N)"

{
  printf 'phase=query\n'
  printf 'query_index=%s\n' "${QUERY_INDEX}"
  printf 'clickbench_query=%s\n' "${CLICKBENCH_QUERY}"
  printf 'columnar=%s\n' "${COLUMNAR}"
  printf 'output=%s\n' "${OUTPUT}"
  printf 'runner=%s\n' "${RUNNER}"
  printf 'start_epoch_ms=%s\n' "${start_ms}"
} > "${LOG_FILE}"

if CLICKBENCH_RUNNER_SKIP_REBUILD=1 "${RUNNER}" "${COLUMNAR}" "${tmp_dir}" "${CLICKBENCH_QUERY}" >"${tmp_stdout}" 2>"${tmp_stderr}"; then
  runner_status="ok"
  runner_exit=0
else
  runner_status="error"
  runner_exit=$?
fi

finish_ms="$(date +%s%3N)"
line=""
if [[ -f "${summary}" ]]; then
  line="$(awk -F, -v q="${CLICKBENCH_QUERY}" 'NR > 1 && $1 == q {print; exit}' "${summary}")"
fi

status="${runner_status}"
rows="0"
query_ms="0.000"
write_ms="0.000"
total_ms="0.000"
read_ms="0.000"
error_message=""

if [[ -n "${line}" ]]; then
  IFS=',' read -r parsed_query status rows query_ms write_ms total_ms parsed_file read_ms parsed_error <<< "${line}"
  error_message="${parsed_error:-}"
fi

if [[ -f "${runner_output}" ]]; then
  mv "${runner_output}" "${OUTPUT}"
elif [[ "${runner_status}" == "ok" ]]; then
  status="unsupported"
  : > "${OUTPUT}"
else
  : > "${OUTPUT}"
fi

if [[ "${runner_status}" != "ok" && -z "${error_message}" ]]; then
  error_message="runner_exit_${runner_exit}"
fi

if [[ ! -f "${GLOBAL_LOG}" ]]; then
  printf 'query_index,clickbench_query,status,runner_exit_code,rows,query_ms,read_ms,write_ms,runner_total_ms,wall_ms,output_bytes,output,log,error\n' > "${GLOBAL_LOG}"
fi

{
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,' \
    "${QUERY_INDEX}" \
    "${CLICKBENCH_QUERY}" \
    "${status}" \
    "${runner_exit}" \
    "${rows}" \
    "${query_ms}" \
    "${read_ms}" \
    "${write_ms}" \
    "${total_ms}" \
    "$((finish_ms - start_ms))" \
    "$(stat -c '%s' "${OUTPUT}" 2>/dev/null || echo 0)"
  printf '"%s","%s","%s"\n' \
    "${OUTPUT//\"/\"\"}" \
    "${LOG_FILE//\"/\"\"}" \
    "${error_message//\"/\"\"}"
} >> "${GLOBAL_LOG}"

{
  printf 'status=%s\n' "${status}"
  printf 'runner_exit_code=%s\n' "${runner_exit}"
  printf 'finish_epoch_ms=%s\n' "${finish_ms}"
  printf 'wall_ms=%s\n' "$((finish_ms - start_ms))"
  printf 'rows=%s\n' "${rows}"
  printf 'query_ms=%s\n' "${query_ms}"
  printf 'read_ms=%s\n' "${read_ms}"
  printf 'write_ms=%s\n' "${write_ms}"
  printf 'runner_total_ms=%s\n' "${total_ms}"
  if [[ -f "${COLUMNAR}" ]]; then
    printf 'columnar_bytes=%s\n' "$(stat -c '%s' "${COLUMNAR}" 2>/dev/null || echo 0)"
  fi
  printf 'output_bytes=%s\n' "$(stat -c '%s' "${OUTPUT}" 2>/dev/null || echo 0)"
  printf '\n[runner_summary]\n'
  if [[ -f "${summary}" ]]; then
    cat "${summary}"
  fi
  printf '\n[runner_stdout]\n'
  cat "${tmp_stdout}"
  printf '\n[runner_stderr]\n'
  cat "${tmp_stderr}"
} >> "${LOG_FILE}"

cat "${tmp_stdout}"
cat "${tmp_stderr}" >&2
rm -rf "${tmp_dir}"

exit "${runner_exit}"
