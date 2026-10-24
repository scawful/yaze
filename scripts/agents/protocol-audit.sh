#!/usr/bin/env bash
# protocol-audit.sh
#
# Validates the compact agent protocol wiring:
# - Maintained policy and routing references in AGENTS.md
# - Persona registry <-> prompt file consistency
# - Required routing and coordination scripts/docs present

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

error_count=0

HAS_RG=0
if command -v rg >/dev/null 2>&1; then
  HAS_RG=1
fi

log_ok() {
  echo "OK: $*"
}

log_err() {
  echo "ERROR: $*" >&2
  error_count=$((error_count + 1))
}

require_file() {
  local path="$1"
  if [[ -f "$path" ]]; then
    log_ok "file exists: $path"
  else
    log_err "missing file: $path"
  fi
}

require_exec() {
  local path="$1"
  if [[ -x "$path" ]]; then
    log_ok "executable exists: $path"
  else
    log_err "missing executable: $path"
  fi
}

require_pattern() {
  local file="$1"
  local pattern="$2"
  local label="$3"
  if (( HAS_RG )); then
    if rg -q "$pattern" "$file"; then
      log_ok "$label"
    else
      log_err "$label (pattern not found: $pattern in $file)"
    fi
    return
  fi
  if grep -Eq "$pattern" "$file"; then
    log_ok "$label"
  else
    log_err "$label (pattern not found: $pattern in $file)"
  fi
}

check_active_doc_board_refs() {
  local hits
  if (( HAS_RG )); then
    hits="$(rg -n "coordination-board\\.md" docs/internal/agents --glob '!docs/internal/agents/archive/**' || true)"
  else
    hits="$(
      find docs/internal/agents -type f ! -path 'docs/internal/agents/archive/*' -print0 \
        | xargs -0 grep -nH "coordination-board\\.md" 2>/dev/null || true
    )"
  fi
  if [[ -z "$hits" ]]; then
    log_ok "no active-doc references to coordination-board.md"
    return
  fi

  local filtered
  if (( HAS_RG )); then
    filtered="$(echo "$hits" | rg -v '^docs/internal/agents/(README\.md|universe-coordination-spec\.md|coordination-board\.md):' || true)"
  else
    filtered="$(echo "$hits" | grep -Ev '^docs/internal/agents/(README\.md|universe-coordination-spec\.md|coordination-board\.md):' || true)"
  fi
  if [[ -n "$filtered" ]]; then
    log_err "unexpected active-doc references to coordination-board.md found:\n$filtered"
  else
    log_ok "active-doc coordination-board.md references are limited to approved files"
  fi
}

check_no_manual_board_instructions() {
  # Scan active (non-archive) agent docs for language that instructs agents
  # to manually edit coordination-board.md. Allowed references are:
  #   - coordination-board.md itself (legacy header)
  #   - universe-coordination-spec.md (migration plan references)
  #   - routing docs that mention it as legacy/history
  #   - README.md (documentation)
  #   - coordination-board.generated.md (the generated snapshot)
  local hits
  if (( HAS_RG )); then
    hits="$(rg -n 'add.*entry.*coordination.board\|update.*coordination.board\|post.*to.*coordination.board\|edit.*coordination.board' \
      docs/internal/agents \
      --glob '!docs/internal/agents/archive/**' \
      --glob '!docs/internal/agents/coordination-board.md' \
      --glob '!docs/internal/agents/coordination-board.generated.md' \
      -i || true)"
  else
    hits="$(
      find docs/internal/agents -type f \
        ! -path 'docs/internal/agents/archive/*' \
        ! -path 'docs/internal/agents/coordination-board.md' \
        ! -path 'docs/internal/agents/coordination-board.generated.md' -print0 \
        | xargs -0 grep -nHiE 'add.*entry.*coordination.board|update.*coordination.board|post.*to.*coordination.board|edit.*coordination.board' 2>/dev/null || true
    )"
  fi
  if [[ -z "$hits" ]]; then
    log_ok "no active docs instruct manual coordination-board writes"
  else
    log_err "active docs still instruct manual board writes:\n$hits"
  fi
}

main() {
  cd "$ROOT_DIR"

  require_file "AGENTS.md"
  require_file "docs/internal/agents/personas.md"
  require_file "docs/internal/agents/routing-personas.md"
  require_file "docs/internal/agents/routing-skills-tools.md"
  require_file "docs/internal/agents/universe-coordination-spec.md"
  require_file "docs/internal/agents/operating-rules.md"
  require_file "docs/internal/architecture/refactor-quality-guardrails.md"

  require_exec "scripts/agents/coord"
  require_exec "scripts/agents/coord-heartbeat.sh"
  require_exec "scripts/agents/universe-coord.sh"
  require_exec "scripts/agents/import-coordination-board.sh"
  require_exec "scripts/agents/migrate-coordination-board.sh"
  require_exec "scripts/agents/test-universe-coord.sh"

  require_pattern "AGENTS.md" "~/src/AGENTS[.]md" "AGENTS inherits workspace instructions"
  require_pattern "AGENTS.md" "docs/internal/agents/operating-rules[.]md" "AGENTS references operating rules"
  require_pattern "AGENTS.md" "docs/internal/architecture/refactor-quality-guardrails[.]md" "AGENTS references architecture guardrails"
  require_pattern "AGENTS.md" "routing-skills-tools.md" "AGENTS references skills/tools routing"
  require_pattern "AGENTS.md" "scripts/agents/coord" "AGENTS references universe coordination wrapper"
  check_active_doc_board_refs

  python3 - <<'PY' || error_count=$((error_count + 1))
import pathlib
import re
import sys

root = pathlib.Path(".")
personas_path = root / "docs/internal/agents/personas.md"
agents_dir = root / ".claude/agents"

if not personas_path.exists():
    print("ERROR: personas.md missing", file=sys.stderr)
    sys.exit(1)
if not agents_dir.exists():
    print("OK: .claude/agents missing; skipping prompt-file audit")
    sys.exit(0)

text = personas_path.read_text(encoding="utf-8")
persona_ids = set(re.findall(r"`([A-Za-z0-9_-]+)`", text))

errors = []

# Every .claude/agents/<id>.md must be declared in personas.md
for p in sorted(agents_dir.glob("*.md")):
    agent_id = p.stem
    if agent_id not in persona_ids:
        errors.append(f"prompt file not declared in personas.md: {p}")
        continue
    body = p.read_text(encoding="utf-8")
    if not re.search(rf"(?m)^name:\s*{re.escape(agent_id)}\s*$", body):
        errors.append(f"front-matter name mismatch in {p}: expected 'name: {agent_id}'")
    if "Protocol Override (Layered Context)" not in body:
        errors.append(f"missing protocol override marker in {p}")

# Lowercase/hyphen IDs in personas.md should have prompt files.
for pid in sorted(persona_ids):
    if re.fullmatch(r"[a-z0-9-]+", pid):
        prompt = agents_dir / f"{pid}.md"
        if not prompt.exists():
            errors.append(f"persona id missing prompt file: {pid} ({prompt})")

if errors:
    for e in errors:
        print(f"ERROR: {e}", file=sys.stderr)
    sys.exit(1)

print("OK: persona registry and prompt files are consistent")
PY

  # ── Semantic check: task-class / persona mapping consistency ──
  python3 - <<'PY' || error_count=$((error_count + 1))
import pathlib
import re
import sys

root = pathlib.Path(".")
tools_md = root / "docs/internal/agents/routing-skills-tools.md"
routing_md = root / "docs/internal/agents/routing-personas.md"
personas_md = root / "docs/internal/agents/personas.md"

errors = []

# Task classes live in the focused routing tables, not the compact router.
tools_text = tools_md.read_text(encoding="utf-8")
tools_classes = set(re.findall(r"\|\s*`([a-z_]+)`\s*\|", tools_text))
persona_ids = set(re.findall(
    r"\|\s*`([A-Za-z0-9_-]+)`\s*\|", personas_md.read_text(encoding="utf-8")
))

# Extract task classes and primary personas from routing-personas.md table rows
routing_text = routing_md.read_text(encoding="utf-8")
# Table rows: | `task_class` | `persona` | ... |
routing_rows = re.findall(
    r"\|\s*`([a-z_]+)`\s*\|\s*`([a-z0-9-]+)`\s*\|", routing_text
)
routing_classes = set()
for task_class, persona in routing_rows:
    routing_classes.add(task_class)

# Both focused routing tables must cover the same task classes.
if not tools_classes or not routing_classes:
    errors.append("task-class routing tables must not be empty")
missing_in_routing = tools_classes - routing_classes
missing_in_tools = routing_classes - tools_classes
if missing_in_routing:
    errors.append(
        f"task classes in routing-skills-tools.md but not in routing-personas.md: "
        f"{sorted(missing_in_routing)}"
    )
if missing_in_tools:
    errors.append(
        f"task classes in routing-personas.md but not in routing-skills-tools.md: "
        f"{sorted(missing_in_tools)}"
    )

# Every primary persona must be declared in the maintained registry.
all_personas_in_routing = set(p for _, p in routing_rows)
for persona in sorted(all_personas_in_routing):
    if persona not in persona_ids:
        errors.append(f"routing persona not declared in personas.md: {persona}")

if errors:
    for e in errors:
        print(f"ERROR: {e}", file=sys.stderr)
    sys.exit(1)

print(f"OK: task-class mapping consistent "
      f"({len(tools_classes)} classes, {len(all_personas_in_routing)} personas)")
PY

  # ── Semantic check: no active docs instruct manual board writes ──
  check_no_manual_board_instructions

  if (( error_count > 0 )); then
    echo "FAIL: protocol audit found $error_count issue(s)" >&2
    exit 1
  fi

  echo "PASS: protocol audit"
}

main "$@"
