#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

exec python3 "$PROJECT_ROOT/validation/board/pcie/scripts/python/run_pcie_validation.py" "$@"
