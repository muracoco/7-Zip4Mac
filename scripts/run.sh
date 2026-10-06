#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
open -a "$DEFAULT_BUILD/7-Zip Mac.app" --args "$@"
