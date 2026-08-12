#!/usr/bin/env bash
set -euo pipefail

tools_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(dirname -- "${tools_dir}")"

exec uv run --project "${project_dir}" python "${tools_dir}/makegif.py" "$@"
