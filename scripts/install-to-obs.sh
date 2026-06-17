#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${root_dir}/build"
plugin_dir="${HOME}/Library/Application Support/obs-studio/plugins"
bundle_name="obs-audio-recorder.plugin"
source_bundle="${build_dir}/${bundle_name}"
target_bundle="${plugin_dir}/${bundle_name}"

if [[ ! -d "${source_bundle}" ]]; then
  echo "Build the plugin first: ${source_bundle} does not exist" >&2
  exit 1
fi

mkdir -p "${plugin_dir}"
rm -rf "${target_bundle}"
cp -R "${source_bundle}" "${target_bundle}"
echo "Installed to ${target_bundle}"

