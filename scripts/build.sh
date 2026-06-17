#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${root_dir}/build"

cmake -S "${root_dir}" -B "${build_dir}" \
  -DQTBASE_PREFIX="/usr/local/opt/qtbase"

cmake --build "${build_dir}" --config Release --target obs-audio-recorder
