#!/usr/bin/env bash
set -euo pipefail

"$(dirname "${BASH_SOURCE[0]}")/build.sh"
"$(dirname "${BASH_SOURCE[0]}")/install-to-obs.sh"
open -a /Applications/OBS.app

