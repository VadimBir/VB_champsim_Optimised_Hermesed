#!/usr/bin/env bash
# Usage: ./query.sh [db_path]
DB="${1:-${CHAMPSIM_DB:-champsim_results.db}}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "${SCRIPT_DIR}/query.py" "$DB"
