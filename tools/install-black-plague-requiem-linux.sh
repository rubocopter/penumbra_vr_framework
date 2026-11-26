#!/usr/bin/env bash
# Shared portable helper; game identity and payload are pinned by the package.
set -euo pipefail
command -v python3 >/dev/null || { echo 'Python 3.9+ is required (standard library only).' >&2; exit 1; }
overlay_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
exec python3 "$overlay_root/install-linux.py" "$@"
