#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

PYTHON_BIN="${PYTHON:-}"

python_has_numpy() {
	"$1" - <<'PY' >/dev/null 2>&1
import numpy  # noqa: F401
PY
}

if [[ -z "$PYTHON_BIN" ]]; then
	candidates=(
		"$(command -v python3 || true)"
		"/root/anaconda3/bin/python3"
		"$HOME/anaconda3/bin/python3"
	)

	if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
		candidates+=("/home/${SUDO_USER}/anaconda3/bin/python3")
	fi

	for candidate in "${candidates[@]}"; do
		if [[ -n "$candidate" && -x "$candidate" ]] && python_has_numpy "$candidate"; then
			PYTHON_BIN="$candidate"
			break
		fi
	done
fi

if [[ -z "$PYTHON_BIN" ]]; then
	echo "[ERROR] No Python interpreter with numpy found." >&2
	echo "        Try: PYTHON=/root/anaconda3/bin/python3 $0 $*" >&2
	exit 127
fi

exec "$PYTHON_BIN" "$PROJECT_ROOT/validation/board/pcie/scripts/python/run_pcie_validation.py" "$@"
