#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

log() {
  printf '[setup] %s\n' "$*"
}

run_as_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    log "skip privileged command, sudo is not available: $*"
    return 0
  fi
}

log "repo=${ROOT_DIR}"
log "kernel=$(uname -srmo 2>/dev/null || true)"
log "cmake=$(cmake --version 2>/dev/null | head -n 1 || true)"
log "cxx=$(${CXX:-c++} --version 2>/dev/null | head -n 1 || true)"

if command -v apt-get >/dev/null 2>&1; then
  export DEBIAN_FRONTEND=noninteractive
  run_as_root apt-get update
  run_as_root apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    git \
    libabsl-dev \
    libboost-dev \
    libgtest-dev \
    libre2-dev \
    make \
    ninja-build \
    pkg-config
elif command -v pacman >/dev/null 2>&1; then
  run_as_root pacman -Sy --needed --noconfirm \
    base-devel \
    cmake \
    git \
    abseil-cpp \
    boost \
    gtest \
    re2
else
  log "unknown package manager; assuming dependencies are already installed"
fi

log "done"
