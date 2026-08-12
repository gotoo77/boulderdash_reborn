#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if ! command -v uv >/dev/null 2>&1; then
    echo "Erreur : UV est requis. Consultez https://docs.astral.sh/uv/" >&2
    exit 1
fi

exec uv run --project "${project_dir}" python "${project_dir}/manage.py" serve-web "$@"
