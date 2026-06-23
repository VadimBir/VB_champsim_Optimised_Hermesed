#!/usr/bin/env bash
# repo_push.sh - add + commit + push the repo to GitHub (origin / current branch).
# Mirrors 000-Repo_Operate.sh func_do_acp, but standalone, non-interactive,
# pushes to the CURRENT branch (not hardcoded main), and filters files >100MB.
#
# Usage:
#   ./repo_push.sh "commit message"          # add . , drop >100MB, commit, push
#   ./repo_push.sh -m "commit message"       # same
#   ./repo_push.sh -n "commit message"       # dry-run: stage + show, no commit/push
set -euo pipefail

DRY=0
MSG=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    -m) MSG="${2:-}"; shift 2 ;;
    -n) DRY=1; shift ;;
    -h|--help) echo "Usage: $0 [-n] [-m] \"commit message\""; exit 0 ;;
    *)  MSG="$1"; shift ;;
  esac
done

[[ -z "$MSG" ]] && { echo "Error: commit message required"; echo "Usage: $0 \"commit message\""; exit 1; }

cd "$(git rev-parse --show-toplevel)"
BRANCH="$(git rev-parse --abbrev-ref HEAD)"
echo "[+] repo : $(git remote get-url origin)"
echo "[+] branch: $BRANCH"

echo "[+] git add ."
git add .

echo "[+] dropping files >100MB from staging"
LARGE="$(find . -size +100M -type f -not -path './.git/*')"
if [[ -n "$LARGE" ]]; then
  while IFS= read -r f; do
    [[ -z "$f" ]] && continue
    if git ls-files --cached -- "$f" | grep -q .; then
      echo "    UNSTAGING: $f (>100MB)"
      git reset -q HEAD -- "$f"
    else
      echo "    skip: $f (>100MB, not staged / ignored)"
    fi
  done <<< "$LARGE"
else
  echo "    none"
fi

echo "[+] staged summary:"
git status --short

if [[ $DRY -eq 1 ]]; then
  echo "[+] dry-run: nothing committed or pushed"
  exit 0
fi

echo "[+] git commit -m \"$MSG\""
git commit -m "$MSG"

echo "[+] git push origin $BRANCH"
git push origin "$BRANCH"
echo "[+] done"
