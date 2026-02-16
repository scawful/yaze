#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! command -v rg >/dev/null 2>&1; then
  echo "error: ripgrep (rg) is required" >&2
  exit 1
fi

count_impl_methods() {
  local file="$1"
  local class_name="$2"
  rg -n "^[A-Za-z_][A-Za-z0-9_:<>*&[:space:]]+${class_name}::[A-Za-z_][A-Za-z0-9_]*\\(" "$file" \
    | wc -l | tr -d ' '
}

count_public_decl_methods() {
  local header="$1"
  local class_name="$2"
  awk -v class_name="$class_name" '
    BEGIN { in_class=0; in_public=0; count=0 }
    $0 ~ ("^class " class_name) { in_class=1; next }
    in_class && /^};/ { in_class=0; in_public=0 }
    in_class && /^ public:/ { in_public=1; next }
    in_class && /^ private:/ { in_public=0 }
    in_class && /^ protected:/ { in_public=0 }
    in_public && /\(/ && /;$/ { count++ }
    END { print count }
  ' "$header"
}

print_hot_methods() {
  local file="$1"
  local class_name="$2"
  local top_n="${3:-8}"
  local total_lines
  total_lines="$(wc -l < "$file" | tr -d ' ')"

  rg -n "^[A-Za-z_][A-Za-z0-9_:<>*&[:space:]]+${class_name}::[A-Za-z_][A-Za-z0-9_]*\\(" "$file" \
    | awk -F: -v tot="$total_lines" '
      BEGIN { prev_line=0; prev_name="" }
      {
        line=$1
        name=$0
        sub(/^[0-9]+:/, "", name)
        sub(/\(.*/, "", name)
        if (prev_line > 0) {
          printf "%s|%d|%d\n", prev_name, prev_line, line - prev_line
        }
        prev_line=line
        prev_name=name
      }
      END {
        if (prev_line > 0) {
          printf "%s|%d|%d\n", prev_name, prev_line, tot - prev_line + 1
        }
      }
    ' \
    | sort -t'|' -k3,3nr \
    | head -n "$top_n"
}

echo "# GUI Complexity Baseline"
echo
echo "## File Metrics"
echo "| File | LOC | Includes | Impl Methods | Public API Decls |"
echo "| --- | ---: | ---: | ---: | ---: |"

declare -a METRIC_ROWS=(
  "src/app/editor/editor_manager.cc|EditorManager|src/app/editor/editor_manager.h|EditorManager"
  "src/app/editor/ui/ui_coordinator.cc|UICoordinator|src/app/editor/ui/ui_coordinator.h|UICoordinator"
  "src/app/editor/system/panel_manager.cc|PanelManager|src/app/editor/system/panel_manager.h|PanelManager"
  "src/app/editor/menu/right_panel_manager.cc|RightPanelManager|src/app/editor/menu/right_panel_manager.h|RightPanelManager"
  "src/app/editor/overworld/tile16_editor.cc|Tile16Editor||"
  "src/app/editor/overworld/overworld_editor.cc|OverworldEditor||"
  "src/app/service/imgui_test_harness_service.cc|ImGuiTestHarnessServiceImpl||"
  "src/app/editor/dungeon/dungeon_canvas_viewer.cc|DungeonCanvasViewer||"
  "src/app/editor/dungeon/dungeon_object_selector.cc|DungeonObjectSelector||"
  "src/app/gui/automation/widget_id_registry.cc|||"
  "src/app/service/widget_discovery_service.cc|||"
)

for row in "${METRIC_ROWS[@]}"; do
  IFS='|' read -r file impl_class header decl_class <<< "$row"
  loc="$(wc -l < "$file" | tr -d ' ')"
  includes="$(rg -c '^#include ' "$file")"

  impl_methods="n/a"
  if [[ -n "$impl_class" ]]; then
    impl_methods="$(count_impl_methods "$file" "$impl_class")"
  fi

  public_decls="n/a"
  if [[ -n "$header" && -n "$decl_class" ]]; then
    public_decls="$(count_public_decl_methods "$header" "$decl_class")"
  fi

  printf '| `%s` | %s | %s | %s | %s |\n' \
    "$file" "$loc" "$includes" "$impl_methods" "$public_decls"
done

echo
echo "## Hot Methods (Approx Span)"
echo

declare -a HOT_TARGETS=(
  "src/app/editor/editor_manager.cc|EditorManager|10"
  "src/app/editor/overworld/tile16_editor.cc|Tile16Editor|8"
  "src/app/editor/overworld/overworld_editor.cc|OverworldEditor|8"
  "src/app/service/imgui_test_harness_service.cc|ImGuiTestHarnessServiceImpl|8"
)

for target in "${HOT_TARGETS[@]}"; do
  IFS='|' read -r file class_name top_n <<< "$target"
  printf '### %s (`%s`)\n' "$class_name" "$file"
  echo "| Method | Start Line | Approx Span |"
  echo "| --- | ---: | ---: |"
  print_hot_methods "$file" "$class_name" "$top_n" \
    | awk -F'|' '{ printf("| `%s` | %s | %s |\n", $1, $2, $3) }'
  echo
done

echo "## Test/AUTOMATION Fragility Signals"
echo "- e2e ItemClick calls: $(rg -n 'ItemClick\(' test/e2e | wc -l | tr -d ' ')"
echo "- e2e ItemExists calls: $(rg -n 'ItemExists\(' test/e2e | wc -l | tr -d ' ')"
echo "- e2e WindowInfo calls: $(rg -n 'WindowInfo\(' test/e2e | wc -l | tr -d ' ')"
echo "- AutoWidgetScope usage in src/app: $(rg -n 'AutoWidgetScope' src/app | wc -l | tr -d ' ')"
echo "- Auto-register helper calls in src/app: $(rg -n 'AutoRegisterLastItem\(|AutoButton\(|AutoCheckbox\(|AutoInputText\(' src/app | wc -l | tr -d ' ')"
