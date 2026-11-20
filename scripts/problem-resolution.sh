#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || {
  echo "ERROR: execute a partir de um checkout Git." >&2
  exit 2
}
cd "$ROOT"

usage() {
  cat <<'USAGE'
Uso:
  scripts/problem-resolution.sh start C-01 <slug> [base]
  scripts/problem-resolution.sh finish C-01 "<commit e PR title>" --paths <path>...

start:
  Atualiza a base e cria fix/c-01-<slug>. Exige worktree limpo.

finish:
  Executa o perfil de testes, faz stage apenas dos caminhos indicados,
  verifica o index, cria commit, faz push, abre PR, espera pelos checks
  do GitHub Actions e só então faz squash merge e apaga a branch remota.
USAGE
}

command_name="${1:-}"
[[ -n "$command_name" ]] || { usage >&2; exit 2; }
shift

normalize_problem_id() {
  local value="${1:-}"
  value="${value^^}"
  [[ "$value" =~ ^C-[0-9]+$ ]] || {
    echo "ERROR: identificador esperado no formato C-NN; recebido: $value" >&2
    exit 2
  }
  printf '%s' "$value"
}

case "$command_name" in
  start)
    problem_id="$(normalize_problem_id "${1:-}")"
    slug="${2:-}"
    base="${3:-main}"
    [[ "$slug" =~ ^[A-Za-z0-9][A-Za-z0-9-]*$ ]] || {
      echo "ERROR: slug inválido: $slug" >&2
      exit 2
    }

    if [[ -n "$(git status --porcelain)" ]]; then
      echo "ERROR: worktree não está limpo. Preserve ou resolva as alterações antes de criar a branch." >&2
      exit 2
    fi

    git fetch origin "$base"
    git switch "$base"
    git pull --ff-only origin "$base"

    branch="fix/${problem_id,,}-${slug,,}"
    if git show-ref --verify --quiet "refs/heads/$branch"; then
      echo "ERROR: branch local já existe: $branch" >&2
      exit 2
    fi
    git switch -c "$branch"
    echo "PROBLEM_BRANCH_CREATED=$branch"
    echo "BASE_COMMIT=$(git rev-parse HEAD)"
    ;;

  finish)
    problem_id="$(normalize_problem_id "${1:-}")"
    title="${2:-}"
    shift 2 || true
    [[ -n "$title" ]] || { echo "ERROR: título de commit/PR obrigatório." >&2; exit 2; }
    [[ "${1:-}" == "--paths" ]] || {
      echo "ERROR: indique os caminhos intencionais com --paths." >&2
      exit 2
    }
    shift
    (($# > 0)) || { echo "ERROR: não foram indicados caminhos para stage." >&2; exit 2; }

    branch="$(git branch --show-current)"
    [[ "$branch" == "fix/${problem_id,,}-"* ]] || {
      echo "ERROR: branch atual '$branch' não corresponde a $problem_id." >&2
      exit 2
    }

    paths=("$@")
    for path in "${paths[@]}"; do
      [[ "$path" != /* && "$path" != *".."* && -e "$ROOT/$path" ]] || {
        echo "ERROR: caminho inválido ou inexistente: $path" >&2
        exit 2
      }
    done

    echo "===== LOCAL VALIDATION: $problem_id ====="
    scripts/problem-tests.sh "$branch"
    git diff --check

    echo "===== SELECTIVE STAGE ====="
    git add -- "${paths[@]}"
    git diff --cached --check

    staged="$(git diff --cached --name-only | sort)"
    expected="$(printf '%s\n' "${paths[@]}" | sort)"
    [[ "$staged" == "$expected" ]] || {
      echo "ERROR: o index contém caminhos diferentes da allowlist." >&2
      echo "STAGED:" >&2
      printf '%s\n' "$staged" >&2
      echo "EXPECTED:" >&2
      printf '%s\n' "$expected" >&2
      exit 2
    }

    git commit -m "$title"
    git push --set-upstream origin "$branch"

    echo "===== OPEN PULL REQUEST ====="
    existing_pr="$(gh pr list --head "$branch" --base main --state open --json number --jq '.[0].number // empty')"
    if [[ -n "$existing_pr" ]]; then
      pr_number="$existing_pr"
    else
      gh pr create \
        --base main \
        --head "$branch" \
        --title "$title" \
        --body "$(cat <<BODY
## Problem
$problem_id

## Validation
- Local problem profile: \`scripts/problem-tests/$problem_id.sh\`
- GitHub Actions: \`Problem resolution gates\`
- Selective staging and \`git diff --cached --check\` completed before commit.

## Scope
Only the explicitly listed files were staged. No unrelated worktree content was included.

## Merge gate
Squash merge is performed by this script only after the PR checks complete successfully.
BODY
)"
      pr_number="$(gh pr list --head "$branch" --base main --state open --json number --jq '.[0].number')"
    fi

    echo "PULL_REQUEST_NUMBER=$pr_number"
    gh pr checks "$pr_number" --watch

    echo "===== SQUASH MERGE ====="
    gh pr merge "$pr_number" --squash --delete-branch
    gh pr view "$pr_number" --json url,state,mergedAt,mergeCommit
    ;;

  *)
    usage >&2
    exit 2
    ;;
esac
