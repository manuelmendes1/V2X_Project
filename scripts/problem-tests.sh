#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || {
  echo "ERROR: execute a partir de um checkout Git do repositório." >&2
  exit 2
}
BRANCH="${1:-$(git -C "$ROOT" branch --show-current)}"

if [[ "$BRANCH" =~ ^fix/(c-[0-9]+)- ]]; then
  PROBLEM_ID="${BASH_REMATCH[1]^^}"
else
  echo "ERROR: branch fora do contrato fix/c-NN-<slug>: $BRANCH" >&2
  exit 2
fi

PROFILE="$ROOT/scripts/problem-tests/${PROBLEM_ID}.sh"
if [[ ! -f "$PROFILE" || ! -x "$PROFILE" ]]; then
  echo "ERROR: falta um perfil executável de testes: scripts/problem-tests/${PROBLEM_ID}.sh" >&2
  echo "Crie o perfil com os testes RED/GREEN específicos antes de abrir o PR." >&2
  exit 2
fi

echo "PROBLEM_ID=$PROBLEM_ID"
echo "PROBLEM_BRANCH=$BRANCH"
"$PROFILE"
