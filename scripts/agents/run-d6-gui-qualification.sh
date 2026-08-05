#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/agents/run-d6-gui-qualification.sh \
    --project <disposable Oracle-of-Secrets.yaze> \
    --rom <disposable oos168.sfc> \
    --canonical-project <protected Oracle-of-Secrets.yaze> \
    --canonical-rom <protected oos168.sfc> \
    [--yaze-bin <path>] [--z3ed-bin <path>] \
    [--harness-port <port>] [--api-port <port>] \
    [--evidence-dir <new-or-empty-directory>]

The disposable project must resolve exactly to --rom. Its ROM must begin as a
byte-identical, non-hard-linked copy of --canonical-rom. The canonical project
and ROM are hash-checked throughout and are never passed to Yaze or z3ed. The
HTTP API port is isolated and reported, but HTTP is not used by this run.
EOF
}

die() {
  local code="$1"
  shift
  echo "error: $*" >&2
  exit "$code"
}

require_value() {
  [[ $# -ge 2 && -n "$2" ]] || die 64 "missing value for $1"
}

PROJECT=""
ROM=""
CANONICAL_PROJECT=""
CANONICAL_ROM=""
YAZE_BIN=""
Z3ED_BIN=""
HARNESS_PORT=50052
API_PORT=51052
EVIDENCE_DIR=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project)
      require_value "$@"
      PROJECT="$2"
      shift 2
      ;;
    --project=*) PROJECT="${1#*=}"; shift ;;
    --rom)
      require_value "$@"
      ROM="$2"
      shift 2
      ;;
    --rom=*) ROM="${1#*=}"; shift ;;
    --canonical-project)
      require_value "$@"
      CANONICAL_PROJECT="$2"
      shift 2
      ;;
    --canonical-project=*) CANONICAL_PROJECT="${1#*=}"; shift ;;
    --canonical-rom)
      require_value "$@"
      CANONICAL_ROM="$2"
      shift 2
      ;;
    --canonical-rom=*) CANONICAL_ROM="${1#*=}"; shift ;;
    --yaze-bin)
      require_value "$@"
      YAZE_BIN="$2"
      shift 2
      ;;
    --yaze-bin=*) YAZE_BIN="${1#*=}"; shift ;;
    --z3ed-bin)
      require_value "$@"
      Z3ED_BIN="$2"
      shift 2
      ;;
    --z3ed-bin=*) Z3ED_BIN="${1#*=}"; shift ;;
    --harness-port)
      require_value "$@"
      HARNESS_PORT="$2"
      shift 2
      ;;
    --harness-port=*) HARNESS_PORT="${1#*=}"; shift ;;
    --api-port)
      require_value "$@"
      API_PORT="$2"
      shift 2
      ;;
    --api-port=*) API_PORT="${1#*=}"; shift ;;
    --evidence-dir)
      require_value "$@"
      EVIDENCE_DIR="$2"
      shift 2
      ;;
    --evidence-dir=*) EVIDENCE_DIR="${1#*=}"; shift ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      die 64 "unknown argument: $1"
      ;;
  esac
done

[[ -n "$PROJECT" ]] || die 64 "--project is required"
[[ -n "$ROM" ]] || die 64 "--rom is required"
[[ -n "$CANONICAL_PROJECT" ]] || die 64 "--canonical-project is required"
[[ -n "$CANONICAL_ROM" ]] || die 64 "--canonical-rom is required"

for command_name in awk cp date find git jq lsof mktemp mv python3 shasum tr; do
  command -v "$command_name" >/dev/null 2>&1 ||
    die 69 "required command not found: $command_name"
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SCRIPT_PATH="$SCRIPT_DIR/run-d6-gui-qualification.sh"
EDIT_FIXTURE="$REPO_ROOT/test/fixtures/gui/d6_edit_and_save.json"
REOPEN_FIXTURE="$REPO_ROOT/test/fixtures/gui/d6_reopen_assert.json"

canonical_path() {
  python3 - "$1" <<'PY'
import os
import sys

path = os.path.realpath(sys.argv[1])
if not os.path.exists(path):
    raise SystemExit(1)
print(path)
PY
}

normalized_path() {
  python3 - "$1" <<'PY'
import os
import sys

print(os.path.realpath(sys.argv[1]))
PY
}

path_is_within() {
  python3 - "$1" "$2" <<'PY'
import os
import sys

child = os.path.realpath(sys.argv[1])
parent = os.path.realpath(sys.argv[2])
try:
    inside = os.path.commonpath([child, parent]) == parent
except ValueError:
    inside = False
raise SystemExit(0 if inside else 1)
PY
}

project_value() {
  python3 - "$1" "$2" "$3" <<'PY'
import configparser
import sys

parser = configparser.ConfigParser(interpolation=None, strict=False)
parser.read(sys.argv[1])
section, key = sys.argv[2], sys.argv[3]
if not parser.has_option(section, key):
    raise SystemExit(1)
print(parser.get(section, key).strip())
PY
}

resolve_project_path() {
  python3 - "$1" "$2" <<'PY'
import os
import sys

project, value = sys.argv[1], sys.argv[2]
print(os.path.realpath(os.path.join(os.path.dirname(project), value)))
PY
}

sha1_file() {
  shasum "$1" | awk '{print $1}'
}

sha256_file() {
  shasum -a 256 "$1" | awk '{print $1}'
}

PROJECT="$(canonical_path "$PROJECT")" || die 66 "project not found"
ROM="$(canonical_path "$ROM")" || die 66 "disposable ROM not found"
CANONICAL_PROJECT="$(canonical_path "$CANONICAL_PROJECT")" ||
  die 66 "canonical project not found"
CANONICAL_ROM="$(canonical_path "$CANONICAL_ROM")" ||
  die 66 "canonical ROM not found"

for required_file in "$PROJECT" "$ROM" "$CANONICAL_PROJECT" \
  "$CANONICAL_ROM" "$EDIT_FIXTURE" "$REOPEN_FIXTURE"; do
  [[ -f "$required_file" ]] || die 66 "required file not found: $required_file"
done

if [[ -z "$YAZE_BIN" ]]; then
  YAZE_BIN="$("$REPO_ROOT/scripts/yaze" --which 2>/dev/null || true)"
fi
if [[ -z "$Z3ED_BIN" ]]; then
  Z3ED_BIN="$("$REPO_ROOT/scripts/z3ed" --which 2>/dev/null || true)"
fi
[[ -n "$YAZE_BIN" ]] || die 69 "Yaze binary not found; pass --yaze-bin"
[[ -n "$Z3ED_BIN" ]] || die 69 "z3ed binary not found; pass --z3ed-bin"
YAZE_BIN="$(canonical_path "$YAZE_BIN")" || die 66 "Yaze binary not found"
Z3ED_BIN="$(canonical_path "$Z3ED_BIN")" || die 66 "z3ed binary not found"
[[ -x "$YAZE_BIN" ]] || die 66 "Yaze binary is not executable: $YAZE_BIN"
[[ -x "$Z3ED_BIN" ]] || die 66 "z3ed binary is not executable: $Z3ED_BIN"

case "$HARNESS_PORT" in
  ''|*[!0-9]*) die 64 "--harness-port must be numeric" ;;
esac
case "$API_PORT" in
  ''|*[!0-9]*) die 64 "--api-port must be numeric" ;;
esac
[[ "$HARNESS_PORT" -ge 1024 && "$HARNESS_PORT" -le 65535 ]] ||
  die 64 "--harness-port must be between 1024 and 65535"
[[ "$API_PORT" -ge 1024 && "$API_PORT" -le 65535 ]] ||
  die 64 "--api-port must be between 1024 and 65535"
[[ "$HARNESS_PORT" -ne "$API_PORT" ]] ||
  die 64 "harness and API ports must differ"

PROJECT_DIR="$(dirname "$PROJECT")"
CANONICAL_PROJECT_DIR="$(dirname "$CANONICAL_PROJECT")"
[[ "$PROJECT" != "$CANONICAL_PROJECT" ]] ||
  die 65 "disposable and canonical project paths are identical"
[[ "$ROM" != "$CANONICAL_ROM" ]] ||
  die 65 "disposable and canonical ROM paths are identical"
if [[ "$PROJECT" -ef "$CANONICAL_PROJECT" ]]; then
  die 65 "disposable project is hard-linked to the canonical project"
fi
if [[ "$ROM" -ef "$CANONICAL_ROM" ]]; then
  die 65 "disposable ROM is hard-linked to the canonical ROM"
fi
if path_is_within "$PROJECT_DIR" "$CANONICAL_PROJECT_DIR"; then
  die 65 "disposable project must not be inside the canonical project tree"
fi
if path_is_within "$CANONICAL_PROJECT_DIR" "$PROJECT_DIR"; then
  die 65 "canonical project must not be inside the disposable project tree"
fi
path_is_within "$ROM" "$PROJECT_DIR" ||
  die 65 "disposable ROM must be inside the disposable project tree"
path_is_within "$CANONICAL_ROM" "$CANONICAL_PROJECT_DIR" ||
  die 65 "canonical ROM does not belong to the canonical project tree"
[[ -w "$ROM" ]] || die 73 "disposable ROM is not writable: $ROM"

TARGET_ROM_REL="$(project_value "$PROJECT" files rom_filename)" ||
  die 65 "disposable project has no files.rom_filename"
TARGET_MANIFEST_REL="$(project_value "$PROJECT" files hack_manifest_file)" ||
  die 65 "disposable project has no files.hack_manifest_file"
TARGET_BACKUP_REL="$(project_value "$PROJECT" files rom_backup_folder)" ||
  die 65 "disposable project has no files.rom_backup_folder"
TARGET_BUILD_REL="$(project_value "$PROJECT" build build_target)" ||
  die 65 "disposable project has no build.build_target"
TARGET_EXPECTED_HASH="$(project_value "$PROJECT" rom expected_hash)" ||
  die 65 "disposable project has no rom.expected_hash"
TARGET_ROLE="$(project_value "$PROJECT" rom role)" ||
  die 65 "disposable project has no rom.role"
TARGET_WRITE_POLICY="$(project_value "$PROJECT" rom write_policy)" ||
  die 65 "disposable project has no rom.write_policy"
TARGET_AUTOSAVE="$(project_value "$PROJECT" workspace autosave_enabled)" ||
  die 65 "disposable project has no workspace.autosave_enabled"
TARGET_BACKUP="$(project_value "$PROJECT" workspace backup_on_save)" ||
  die 65 "disposable project has no workspace.backup_on_save"

TARGET_PROJECT_ROM="$(resolve_project_path "$PROJECT" "$TARGET_ROM_REL")"
TARGET_MANIFEST="$(resolve_project_path "$PROJECT" "$TARGET_MANIFEST_REL")"
TARGET_BACKUP_DIR="$(resolve_project_path "$PROJECT" "$TARGET_BACKUP_REL")"
TARGET_BUILD="$(resolve_project_path "$PROJECT" "$TARGET_BUILD_REL")"
[[ "$TARGET_PROJECT_ROM" == "$ROM" ]] ||
  die 65 "project resolves ROM to $TARGET_PROJECT_ROM, not --rom $ROM"
[[ "$TARGET_BUILD" != "$ROM" ]] ||
  die 65 "project build output aliases its editable ROM"
[[ -f "$TARGET_MANIFEST" ]] ||
  die 66 "project Hack Manifest not found: $TARGET_MANIFEST"
path_is_within "$TARGET_MANIFEST" "$PROJECT_DIR" ||
  die 65 "project Hack Manifest must be inside the disposable project tree"
[[ -n "$TARGET_BACKUP_REL" ]] ||
  die 65 "project rom_backup_folder must not be empty"
path_is_within "$TARGET_BACKUP_DIR" "$PROJECT_DIR" ||
  die 65 "project ROM backup folder must be inside the disposable project tree"
if [[ -e "$TARGET_BACKUP_DIR" ]]; then
  [[ -d "$TARGET_BACKUP_DIR" ]] ||
    die 65 "project ROM backup path is not a directory: $TARGET_BACKUP_DIR"
  [[ -w "$TARGET_BACKUP_DIR" ]] ||
    die 73 "project ROM backup directory is not writable: $TARGET_BACKUP_DIR"
else
  [[ -w "$(dirname "$TARGET_BACKUP_DIR")" ]] ||
    die 73 "project ROM backup parent is not writable: $TARGET_BACKUP_DIR"
fi
path_is_within "$TARGET_BUILD" "$PROJECT_DIR" ||
  die 65 "project build target must be inside the disposable project tree"

CANONICAL_ROM_REL="$(project_value "$CANONICAL_PROJECT" files rom_filename)" ||
  die 65 "canonical project has no files.rom_filename"
CANONICAL_MANIFEST_REL="$(
  project_value "$CANONICAL_PROJECT" files hack_manifest_file
)" || die 65 "canonical project has no files.hack_manifest_file"
CANONICAL_PROJECT_ROM="$(resolve_project_path "$CANONICAL_PROJECT" "$CANONICAL_ROM_REL")"
CANONICAL_MANIFEST="$(
  resolve_project_path "$CANONICAL_PROJECT" "$CANONICAL_MANIFEST_REL"
)"
[[ "$CANONICAL_PROJECT_ROM" == "$CANONICAL_ROM" ]] ||
  die 65 "canonical descriptor does not resolve to --canonical-rom"
[[ -f "$CANONICAL_MANIFEST" ]] ||
  die 66 "canonical Hack Manifest not found: $CANONICAL_MANIFEST"
path_is_within "$CANONICAL_MANIFEST" "$CANONICAL_PROJECT_DIR" ||
  die 65 "canonical Hack Manifest must be inside the canonical project tree"
[[ "$TARGET_MANIFEST" != "$CANONICAL_MANIFEST" ]] ||
  die 65 "disposable and canonical Hack Manifest paths are identical"
if [[ "$TARGET_MANIFEST" -ef "$CANONICAL_MANIFEST" ]]; then
  die 65 "disposable Hack Manifest is hard-linked to the canonical manifest"
fi

[[ "$(printf '%s' "$TARGET_ROLE" | tr '[:upper:]' '[:lower:]')" == "dev" ]] ||
  die 65 "disposable project ROM role must be dev"
[[ "$(printf '%s' "$TARGET_WRITE_POLICY" | tr '[:upper:]' '[:lower:]')" == "block" ]] ||
  die 65 "disposable project write policy must be block"
[[ "$(printf '%s' "$TARGET_AUTOSAVE" | tr '[:upper:]' '[:lower:]')" == "false" ]] ||
  die 65 "disposable project autosave must be disabled"
[[ "$(printf '%s' "$TARGET_BACKUP" | tr '[:upper:]' '[:lower:]')" == "true" ]] ||
  die 65 "disposable project backup_on_save must be enabled"

FEATURE_FLAG_REPORT="$(python3 - "$PROJECT" <<'PY'
import configparser
import json
import sys

parser = configparser.ConfigParser(interpolation=None, strict=False)
parser.read(sys.argv[1])

def parse_bool(key, *, required, default):
    if not parser.has_option("feature_flags", key):
        if required:
            raise SystemExit(f"missing required feature_flags.{key}")
        return {"present": False, "value": default, "source": "yaze_default"}
    raw = parser.get("feature_flags", key).strip().lower()
    if raw in {"true", "1", "yes", "on"}:
        value = True
    elif raw in {"false", "0", "no", "off"}:
        value = False
    else:
        raise SystemExit(f"invalid boolean for feature_flags.{key}: {raw}")
    return {"present": True, "value": value, "source": "project"}

report = {
    "save_dungeon_maps": parse_bool(
        "save_dungeon_maps", required=True, default=False
    ),
    "save_graphics_sheet": parse_bool(
        "save_graphics_sheet", required=True, default=False
    ),
}
for key in (
    "save_dungeon_objects",
    "save_dungeon_sprites",
    "save_dungeon_room_headers",
    "save_dungeon_torches",
    "save_dungeon_pits",
    "save_dungeon_blocks",
    "save_dungeon_collision",
    "save_dungeon_chests",
    "save_dungeon_pot_items",
    "save_dungeon_entrances",
    "save_dungeon_palettes",
):
    report[key] = parse_bool(key, required=False, default=True)

# The native INI reader in this Yaze revision does not parse a
# save_dungeon_water_fill_zones key. Record the actual effective default rather
# than implying a descriptor value can disable it.
water_fill_raw = None
if parser.has_option("feature_flags", "save_dungeon_water_fill_zones"):
    water_fill_raw = parser.get(
        "feature_flags", "save_dungeon_water_fill_zones"
    ).strip()
report["save_dungeon_water_fill_zones"] = {
    "present": water_fill_raw is not None,
    "descriptor_value_ignored": water_fill_raw,
    "value": True,
    "source": "yaze_default_unconfigurable_in_ini",
}

if report["save_dungeon_maps"]["value"]:
    raise SystemExit("feature_flags.save_dungeon_maps must be false")
if report["save_graphics_sheet"]["value"]:
    raise SystemExit("feature_flags.save_graphics_sheet must be false")
for key in (
    "save_dungeon_objects",
    "save_dungeon_sprites",
    "save_dungeon_pot_items",
    "save_dungeon_palettes",
):
    if not report[key]["value"]:
        raise SystemExit(f"feature_flags.{key} must be true when specified")

unrelated_keys = (
    "save_dungeon_room_headers",
    "save_dungeon_torches",
    "save_dungeon_pits",
    "save_dungeon_blocks",
    "save_dungeon_collision",
    "save_dungeon_water_fill_zones",
    "save_dungeon_chests",
    "save_dungeon_entrances",
    "save_dungeon_maps",
    "save_graphics_sheet",
)
payload = {
    "effective_flags": report,
    "edited_domains_required_true": [
        "save_dungeon_objects",
        "save_dungeon_sprites",
        "save_dungeon_pot_items",
        "save_dungeon_palettes",
    ],
    "unrelated_enabled_domains": [
        key for key in unrelated_keys if report[key]["value"]
    ],
    "runtime_override": {
        "applied": False,
        "descriptor_modified": False,
        "policy": (
            "Record effective defaults; exact ROM byte-diff containment must "
            "reject writes from unrelated domains."
        ),
    },
}
print(json.dumps(payload, sort_keys=True))
PY
)" || die 65 "disposable project feature-flag save scope is unsafe"

jq -e '
  .dungeon_stream_regions.objects.strategy == "copy_on_write" and
  .dungeon_stream_regions.sprites.strategy == "copy_on_write" and
  .dungeon_stream_regions.pot_items.strategy == "repack_all"
' "$TARGET_MANIFEST" >/dev/null ||
  die 65 "Hack Manifest lacks the required dungeon stream write strategies"

CANONICAL_ROM_SHA1_BEFORE="$(sha1_file "$CANONICAL_ROM")"
CANONICAL_ROM_SHA256_BEFORE="$(sha256_file "$CANONICAL_ROM")"
CANONICAL_PROJECT_SHA256_BEFORE="$(sha256_file "$CANONICAL_PROJECT")"
CANONICAL_MANIFEST_SHA256_BEFORE="$(sha256_file "$CANONICAL_MANIFEST")"
TARGET_ROM_SHA1_BEFORE="$(sha1_file "$ROM")"
TARGET_ROM_SHA256_BEFORE="$(sha256_file "$ROM")"
TARGET_PROJECT_SHA256_BEFORE="$(sha256_file "$PROJECT")"
TARGET_MANIFEST_SHA256_BEFORE="$(sha256_file "$TARGET_MANIFEST")"
YAZE_BIN_SHA256="$(sha256_file "$YAZE_BIN")"
Z3ED_BIN_SHA256="$(sha256_file "$Z3ED_BIN")"
SCRIPT_SHA256="$(sha256_file "$SCRIPT_PATH")"
EDIT_FIXTURE_SHA256="$(sha256_file "$EDIT_FIXTURE")"
REOPEN_FIXTURE_SHA256="$(sha256_file "$REOPEN_FIXTURE")"
QUALIFIED_BASELINE_SHA256="b13ef69ede313756dcf560bf0f993663d68d0365609f2c20dcdec2c17f31f7b9"
QUALIFIED_SAVED_SHA256="d169961b9b83cddd33f63ce7b4a12c50d11939627c7f04dba8143672a91100e8"
[[ "$TARGET_MANIFEST_SHA256_BEFORE" == "$CANONICAL_MANIFEST_SHA256_BEFORE" ]] ||
  die 65 "disposable Hack Manifest is not byte-identical to the canonical manifest"
MANIFESTS_EQUAL_BEFORE=true
[[ "$TARGET_ROM_SHA1_BEFORE" == "$CANONICAL_ROM_SHA1_BEFORE" &&
   "$TARGET_ROM_SHA256_BEFORE" == "$CANONICAL_ROM_SHA256_BEFORE" ]] ||
  die 65 "disposable ROM is not a byte-identical canonical copy"
NORMALIZED_TARGET_EXPECTED_HASH="$({
  printf '%s' "$TARGET_EXPECTED_HASH" | tr '[:upper:]' '[:lower:]'
})"
[[ "$NORMALIZED_TARGET_EXPECTED_HASH" == "$TARGET_ROM_SHA1_BEFORE" ]] ||
  die 65 "disposable descriptor expected_hash does not match its ROM"
[[ "$TARGET_ROM_SHA256_BEFORE" == "$QUALIFIED_BASELINE_SHA256" ]] ||
  die 65 "disposable ROM is not the qualified D6 baseline SHA-256"

reject_protected_evidence_path() {
  local candidate="$1"
  if path_is_within "$candidate" "$CANONICAL_PROJECT_DIR"; then
    die 65 "evidence directory must not be inside the canonical project tree"
  fi
  if path_is_within "$candidate" "$REPO_ROOT"; then
    die 65 "evidence directory must not be inside the Yaze source tree"
  fi
}

if [[ -z "$EVIDENCE_DIR" ]]; then
  EVIDENCE_BASE="$(normalized_path "${TMPDIR:-/tmp}")"
  [[ -d "$EVIDENCE_BASE" ]] ||
    die 73 "temporary evidence parent does not exist: $EVIDENCE_BASE"
  reject_protected_evidence_path "$EVIDENCE_BASE"
  EVIDENCE_DIR="$(mktemp -d "$EVIDENCE_BASE/yaze-d6-gui-qualification.XXXXXX")"
else
  EVIDENCE_DIR="$(normalized_path "$EVIDENCE_DIR")"
  reject_protected_evidence_path "$EVIDENCE_DIR"
  if [[ -e "$EVIDENCE_DIR" ]]; then
    [[ -d "$EVIDENCE_DIR" ]] || die 73 "evidence path is not a directory"
    [[ -z "$(find "$EVIDENCE_DIR" -mindepth 1 -maxdepth 1 -print -quit)" ]] ||
      die 73 "refusing to overwrite non-empty evidence directory"
  else
    mkdir -p "$EVIDENCE_DIR"
  fi
fi
EVIDENCE_DIR="$(canonical_path "$EVIDENCE_DIR")" ||
  die 73 "could not create evidence directory"
reject_protected_evidence_path "$EVIDENCE_DIR"

FROZEN_INPUT_DIR="$EVIDENCE_DIR/frozen-inputs"
FROZEN_EDIT_FIXTURE="$FROZEN_INPUT_DIR/d6_edit_and_save.json"
FROZEN_REOPEN_FIXTURE="$FROZEN_INPUT_DIR/d6_reopen_assert.json"
mkdir "$FROZEN_INPUT_DIR"
cp "$EDIT_FIXTURE" "$FROZEN_EDIT_FIXTURE"
cp "$REOPEN_FIXTURE" "$FROZEN_REOPEN_FIXTURE"
[[ "$(sha256_file "$FROZEN_EDIT_FIXTURE")" == "$EDIT_FIXTURE_SHA256" ]] ||
  die 74 "frozen edit fixture hash mismatch"
[[ "$(sha256_file "$FROZEN_REOPEN_FIXTURE")" == "$REOPEN_FIXTURE_SHA256" ]] ||
  die 74 "frozen reopen fixture hash mismatch"

FEATURE_FLAG_EVIDENCE="$EVIDENCE_DIR/feature-flags.json"
printf '%s\n' "$FEATURE_FLAG_REPORT" | jq . > "$FEATURE_FLAG_EVIDENCE"
GIT_STATUS_EVIDENCE="$EVIDENCE_DIR/yaze-git-status.txt"
git -C "$REPO_ROOT" status --porcelain=v1 --untracked-files=all \
  > "$GIT_STATUS_EVIDENCE" || die 70 "could not record Yaze git status"
YAZE_GIT_HEAD="$(git -C "$REPO_ROOT" rev-parse HEAD)" ||
  die 70 "could not resolve Yaze git HEAD"
if [[ -s "$GIT_STATUS_EVIDENCE" ]]; then
  YAZE_GIT_DIRTY=true
else
  YAZE_GIT_DIRTY=false
fi

OBJECT_STREAM_INVENTORY="$EVIDENCE_DIR/object-stream-plan.json"
SPRITE_STREAM_INVENTORY="$EVIDENCE_DIR/sprite-stream-plan.json"
STREAM_INVENTORY_VALIDATION="$EVIDENCE_DIR/stream-inventory-validation.json"

capture_stream_inventory() {
  local kind="$1"
  local output="$2"
  NO_COLOR=1 TERM=dumb "$Z3ED_BIN" dungeon-stream-plan \
    --kind="$kind" --manifest="$TARGET_MANIFEST" --rom="$ROM" \
    --format=json > "$output" ||
    die 70 "z3ed $kind stream inventory failed"
}

capture_stream_inventory objects "$OBJECT_STREAM_INVENTORY"
capture_stream_inventory sprites "$SPRITE_STREAM_INVENTORY"
[[ "$(sha256_file "$ROM")" == "$TARGET_ROM_SHA256_BEFORE" ]] ||
  die 74 "read-only stream inventory changed the disposable ROM"
[[ "$(sha256_file "$TARGET_MANIFEST")" == "$TARGET_MANIFEST_SHA256_BEFORE" ]] ||
  die 74 "read-only stream inventory changed the disposable manifest"

python3 - "$ROM" "$TARGET_MANIFEST" "$OBJECT_STREAM_INVENTORY" \
  "$SPRITE_STREAM_INVENTORY" "$STREAM_INVENTORY_VALIDATION" <<'PY'
import hashlib
import json
import os
import sys

rom_path, manifest_path, object_path, sprite_path, output = sys.argv[1:]
rom = open(rom_path, "rb").read()

def digest_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

def parse_hex(value, field):
    if not isinstance(value, str):
        raise SystemExit(f"{field} must be a hexadecimal string")
    try:
        parsed = int(value, 0)
    except ValueError as error:
        raise SystemExit(f"{field} is not hexadecimal: {value!r}") from error
    if parsed < 0:
        raise SystemExit(f"{field} must not be negative")
    return parsed

def validate_inventory(path, expected_kind, target_rooms, expected_crc32):
    with open(path, encoding="utf-8") as stream:
        inventory = json.load(stream)
    if inventory.get("status") != "ok":
        raise SystemExit(f"{expected_kind} stream inventory status is not ok")
    if inventory.get("kind") != expected_kind:
        raise SystemExit(f"{expected_kind} stream inventory kind mismatch")
    if inventory.get("strategy") != "copy_on_write":
        raise SystemExit(f"{expected_kind} stream inventory is not copy_on_write")
    if os.path.realpath(inventory.get("manifest", "")) != os.path.realpath(
        manifest_path
    ):
        raise SystemExit(f"{expected_kind} stream inventory manifest mismatch")
    if inventory.get("source_size") != len(rom):
        raise SystemExit(f"{expected_kind} stream inventory ROM size mismatch")
    if inventory.get("source_crc32") != expected_crc32:
        raise SystemExit(f"{expected_kind} stream inventory ROM CRC32 mismatch")
    if inventory.get("issue_count") != 0 or inventory.get("issues") != []:
        raise SystemExit(f"{expected_kind} stream inventory reports issues")
    if (
        inventory.get("suffix_overlap_count") != 0
        or inventory.get("interior_overlap_count") != 0
        or inventory.get("overlaps") != []
    ):
        raise SystemExit(f"{expected_kind} stream inventory reports overlaps")

    unique_streams = inventory.get("unique_streams")
    exact_aliases = inventory.get("exact_aliases")
    if not isinstance(unique_streams, list) or not isinstance(exact_aliases, list):
        raise SystemExit(f"{expected_kind} stream inventory lists are invalid")

    targets = []
    for room in target_rooms:
        room_id = f"0x{room:03X}"
        matches = [
            entry
            for entry in unique_streams
            if isinstance(entry, dict)
            and isinstance(entry.get("room_ids"), list)
            and room_id in entry["room_ids"]
        ]
        if len(matches) != 1:
            raise SystemExit(
                f"{expected_kind} room {room_id} must have one unique stream"
            )
        entry = matches[0]
        if entry.get("room_ids") != [room_id]:
            raise SystemExit(
                f"{expected_kind} room {room_id} baseline stream is aliased"
            )
        if any(
            isinstance(alias, dict)
            and room_id in alias.get("room_ids", [])
            for alias in exact_aliases
        ):
            raise SystemExit(f"{expected_kind} room {room_id} is exactly aliased")
        start = parse_hex(entry.get("data_pc"), f"{room_id}.data_pc")
        end = parse_hex(entry.get("end_pc"), f"{room_id}.end_pc")
        if not (0 <= start < end <= len(rom)):
            raise SystemExit(f"{expected_kind} room {room_id} span is outside ROM")
        if entry.get("bytes") != end - start:
            raise SystemExit(f"{expected_kind} room {room_id} byte count mismatch")
        targets.append(
            {
                "room_id": room_id,
                "data_pc": f"0x{start:06X}",
                "end_pc": f"0x{end:06X}",
                "bytes": end - start,
                "unique": True,
                "unaliased": True,
                "overlap_free": True,
            }
        )

    return {
        "path": os.path.realpath(path),
        "sha256": digest_file(path),
        "kind": expected_kind,
        "status": "ok",
        "manifest": os.path.realpath(manifest_path),
        "source_size": len(rom),
        "source_crc32": expected_crc32,
        "strategy": "copy_on_write",
        "targets": targets,
    }

with open(object_path, encoding="utf-8") as stream:
    object_crc32 = json.load(stream).get("source_crc32")
with open(sprite_path, encoding="utf-8") as stream:
    sprite_crc32 = json.load(stream).get("source_crc32")
if (
    not isinstance(object_crc32, str)
    or object_crc32 != sprite_crc32
    or not object_crc32.startswith("0x")
):
    raise SystemExit("object/sprite stream inventory source CRC32 mismatch")

result = {
    "status": "pass",
    "rom": {
        "path": os.path.realpath(rom_path),
        "size": len(rom),
        "sha256": hashlib.sha256(rom).hexdigest(),
        "inventory_source_crc32": object_crc32,
        "crc32_proof": (
            "Matching read-only object/sprite inventories captured between "
            "unchanged ROM SHA-256 checks"
        ),
    },
    "manifest": {
        "path": os.path.realpath(manifest_path),
        "sha256": digest_file(manifest_path),
    },
    "inventories": {
        "objects": validate_inventory(
            object_path, "objects", (0xA8, 0xB8), object_crc32
        ),
        "sprites": validate_inventory(
            sprite_path, "sprites", (0xD8,), object_crc32
        ),
    },
}
with open(output, "w", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
PY

OBJECT_STREAM_INVENTORY_SHA256="$(sha256_file "$OBJECT_STREAM_INVENTORY")"
SPRITE_STREAM_INVENTORY_SHA256="$(sha256_file "$SPRITE_STREAM_INVENTORY")"
STREAM_INVENTORY_VALIDATION_SHA256="$({
  sha256_file "$STREAM_INVENTORY_VALIDATION"
})"

PROVENANCE_EVIDENCE="$EVIDENCE_DIR/provenance.json"
jq -n \
  --arg captured_at_utc "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" \
  --arg yaze_git_head "$YAZE_GIT_HEAD" \
  --arg git_status "$GIT_STATUS_EVIDENCE" \
  --arg script_path "$SCRIPT_PATH" \
  --arg script_sha256 "$SCRIPT_SHA256" \
  --arg edit_fixture "$EDIT_FIXTURE" \
  --arg edit_fixture_sha256 "$EDIT_FIXTURE_SHA256" \
  --arg frozen_edit_fixture "$FROZEN_EDIT_FIXTURE" \
  --arg reopen_fixture "$REOPEN_FIXTURE" \
  --arg reopen_fixture_sha256 "$REOPEN_FIXTURE_SHA256" \
  --arg frozen_reopen_fixture "$FROZEN_REOPEN_FIXTURE" \
  --arg yaze_bin "$YAZE_BIN" \
  --arg yaze_bin_sha256 "$YAZE_BIN_SHA256" \
  --arg z3ed_bin "$Z3ED_BIN" \
  --arg z3ed_bin_sha256 "$Z3ED_BIN_SHA256" \
  --arg project "$PROJECT" \
  --arg project_sha256 "$TARGET_PROJECT_SHA256_BEFORE" \
  --arg manifest "$TARGET_MANIFEST" \
  --arg manifest_sha256 "$TARGET_MANIFEST_SHA256_BEFORE" \
  --arg rom "$ROM" \
  --arg rom_sha256 "$TARGET_ROM_SHA256_BEFORE" \
  --arg canonical_project "$CANONICAL_PROJECT" \
  --arg canonical_project_sha256 "$CANONICAL_PROJECT_SHA256_BEFORE" \
  --arg canonical_manifest "$CANONICAL_MANIFEST" \
  --arg canonical_manifest_sha256 "$CANONICAL_MANIFEST_SHA256_BEFORE" \
  --arg canonical_rom "$CANONICAL_ROM" \
  --arg canonical_rom_sha256 "$CANONICAL_ROM_SHA256_BEFORE" \
  --arg feature_flags "$FEATURE_FLAG_EVIDENCE" \
  --arg object_stream_inventory "$OBJECT_STREAM_INVENTORY" \
  --arg object_stream_inventory_sha256 "$OBJECT_STREAM_INVENTORY_SHA256" \
  --arg sprite_stream_inventory "$SPRITE_STREAM_INVENTORY" \
  --arg sprite_stream_inventory_sha256 "$SPRITE_STREAM_INVENTORY_SHA256" \
  --arg stream_inventory_validation "$STREAM_INVENTORY_VALIDATION" \
  --arg stream_inventory_validation_sha256 "$STREAM_INVENTORY_VALIDATION_SHA256" \
  --argjson yaze_git_dirty "$YAZE_GIT_DIRTY" \
  --argjson manifests_equal_before "$MANIFESTS_EQUAL_BEFORE" '
    {
      captured_at_utc: $captured_at_utc,
      yaze_git: {
        head: $yaze_git_head,
        dirty: $yaze_git_dirty,
        status_file: $git_status
      },
      driver: {path: $script_path, sha256: $script_sha256},
      fixtures: {
        edit: {
          source: $edit_fixture,
          frozen: $frozen_edit_fixture,
          sha256: $edit_fixture_sha256
        },
        reopen: {
          source: $reopen_fixture,
          frozen: $frozen_reopen_fixture,
          sha256: $reopen_fixture_sha256
        }
      },
      binaries: {
        yaze: {path: $yaze_bin, sha256: $yaze_bin_sha256},
        z3ed: {path: $z3ed_bin, sha256: $z3ed_bin_sha256}
      },
      disposable: {
        project: {path: $project, sha256: $project_sha256},
        manifest: {path: $manifest, sha256: $manifest_sha256},
        rom: {path: $rom, sha256: $rom_sha256},
        feature_flags: $feature_flags,
        baseline_stream_inventory: {
          objects: {
            path: $object_stream_inventory,
            sha256: $object_stream_inventory_sha256
          },
          sprites: {
            path: $sprite_stream_inventory,
            sha256: $sprite_stream_inventory_sha256
          },
          validation: {
            path: $stream_inventory_validation,
            sha256: $stream_inventory_validation_sha256
          }
        }
      },
      canonical: {
        project: {path: $canonical_project, sha256: $canonical_project_sha256},
        manifest: {path: $canonical_manifest, sha256: $canonical_manifest_sha256},
        rom: {path: $canonical_rom, sha256: $canonical_rom_sha256}
      },
      manifests_equal_before: $manifests_equal_before
    }
  ' > "$PROVENANCE_EVIDENCE"

YAZE_PID=""
FIRST_PID=""
SECOND_PID=""

assert_frozen_inputs_unchanged() {
  [[ -n "${CANONICAL_ROM_SHA256_BEFORE:-}" ]] || return 0
  [[ -f "$CANONICAL_ROM" && -f "$CANONICAL_PROJECT" &&
     -f "$CANONICAL_MANIFEST" && -f "$PROJECT" &&
     -f "$TARGET_MANIFEST" && -f "$SCRIPT_PATH" &&
     -f "$EDIT_FIXTURE" && -f "$REOPEN_FIXTURE" &&
     -f "$FROZEN_EDIT_FIXTURE" && -f "$FROZEN_REOPEN_FIXTURE" &&
     -f "$OBJECT_STREAM_INVENTORY" && -f "$SPRITE_STREAM_INVENTORY" &&
     -f "$STREAM_INVENTORY_VALIDATION" &&
     -f "$YAZE_BIN" && -f "$Z3ED_BIN" ]] || return 1
  [[ "$(sha256_file "$CANONICAL_ROM")" == "$CANONICAL_ROM_SHA256_BEFORE" ]] ||
    return 1
  [[ "$(sha256_file "$CANONICAL_PROJECT")" == "$CANONICAL_PROJECT_SHA256_BEFORE" ]] ||
    return 1
  [[ "$(sha256_file "$CANONICAL_MANIFEST")" == "$CANONICAL_MANIFEST_SHA256_BEFORE" ]] ||
    return 1
  [[ "$(sha256_file "$PROJECT")" == "$TARGET_PROJECT_SHA256_BEFORE" ]] ||
    return 1
  [[ "$(sha256_file "$TARGET_MANIFEST")" == "$TARGET_MANIFEST_SHA256_BEFORE" ]] ||
    return 1
  [[ "$(sha256_file "$SCRIPT_PATH")" == "$SCRIPT_SHA256" ]] || return 1
  [[ "$(sha256_file "$EDIT_FIXTURE")" == "$EDIT_FIXTURE_SHA256" ]] ||
    return 1
  [[ "$(sha256_file "$REOPEN_FIXTURE")" == "$REOPEN_FIXTURE_SHA256" ]] ||
    return 1
  [[ "$(sha256_file "$FROZEN_EDIT_FIXTURE")" == "$EDIT_FIXTURE_SHA256" ]] ||
    return 1
  [[ "$(sha256_file "$FROZEN_REOPEN_FIXTURE")" == "$REOPEN_FIXTURE_SHA256" ]] ||
    return 1
  [[ "$(sha256_file "$OBJECT_STREAM_INVENTORY")" == \
     "$OBJECT_STREAM_INVENTORY_SHA256" ]] || return 1
  [[ "$(sha256_file "$SPRITE_STREAM_INVENTORY")" == \
     "$SPRITE_STREAM_INVENTORY_SHA256" ]] || return 1
  [[ "$(sha256_file "$STREAM_INVENTORY_VALIDATION")" == \
     "$STREAM_INVENTORY_VALIDATION_SHA256" ]] || return 1
  [[ "$(sha256_file "$YAZE_BIN")" == "$YAZE_BIN_SHA256" ]] || return 1
  [[ "$(sha256_file "$Z3ED_BIN")" == "$Z3ED_BIN_SHA256" ]] || return 1
  return 0
}

cleanup() {
  local status=$?
  trap - EXIT
  set +e
  if [[ -n "${YAZE_PID:-}" ]] && kill -0 "$YAZE_PID" 2>/dev/null; then
    kill -TERM "$YAZE_PID" 2>/dev/null
    local i=0
    while kill -0 "$YAZE_PID" 2>/dev/null && [[ "$i" -lt 50 ]]; do
      sleep 0.1
      i=$((i + 1))
    done
    if kill -0 "$YAZE_PID" 2>/dev/null; then
      kill -KILL "$YAZE_PID" 2>/dev/null
    fi
    wait "$YAZE_PID" 2>/dev/null
  fi
  if ! assert_frozen_inputs_unchanged; then
    echo "error: a frozen project, manifest, fixture, script, or binary changed during qualification" >&2
    status=74
  fi
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

port_is_free() {
  python3 - "$1" <<'PY'
import socket
import sys

port = int(sys.argv[1])
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    sock.bind(("127.0.0.1", port))
except OSError:
    raise SystemExit(1)
finally:
    sock.close()
PY
}

wait_for_port_free() {
  local port="$1"
  local deadline=$((SECONDS + 15))
  while [[ "$SECONDS" -lt "$deadline" ]]; do
    if port_is_free "$port"; then
      return 0
    fi
    sleep 0.2
  done
  return 1
}

wait_for_owned_listener() {
  local label="$1"
  local role="$2"
  local port="$3"
  local timeout_seconds="${4:-30}"
  local output="$EVIDENCE_DIR/$label-$role-listener.lsof"
  local metadata="$EVIDENCE_DIR/$label-$role-listener.json"
  local deadline=$((SECONDS + timeout_seconds))
  while [[ "$SECONDS" -lt "$deadline" ]]; do
    if ! kill -0 "$YAZE_PID" 2>/dev/null; then
      return 1
    fi
    if lsof -nP -a -p "$YAZE_PID" -iTCP:"$port" -sTCP:LISTEN \
         > "$output" 2> "$output.stderr" &&
       awk -v pid="$YAZE_PID" '
         NR > 1 && $2 == pid && $NF == "(LISTEN)" { found = 1 }
         END { exit(found ? 0 : 1) }
       ' "$output"; then
      jq -n \
        --arg captured_at_utc "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" \
        --arg label "$label" \
        --arg role "$role" \
        --arg lsof_output "$output" \
        --argjson pid "$YAZE_PID" \
        --argjson port "$port" '
          {
            captured_at_utc: $captured_at_utc,
            launch: $label,
            role: $role,
            pid: $pid,
            port: $port,
            state: "LISTEN",
            ownership: "lsof -a -p PID -iTCP:PORT -sTCP:LISTEN",
            lsof_output: $lsof_output
          }
        ' > "$metadata" || return 1
      return 0
    fi
    sleep 0.2
  done
  return 1
}

record_optional_api_listener() {
  local label="$1"
  local output="$EVIDENCE_DIR/$label-api-listener.lsof"
  local metadata="$EVIDENCE_DIR/$label-api-listener.json"
  local all_listeners="$EVIDENCE_DIR/$label-api-port-listeners.lsof"

  if wait_for_owned_listener "$label" api "$API_PORT" 1; then
    local updated="$metadata.tmp"
    jq '.required_for_qualification = false' "$metadata" > "$updated" ||
      return 1
    mv "$updated" "$metadata" || return 1
    return 0
  fi

  kill -0 "$YAZE_PID" 2>/dev/null || return 1
  if lsof -nP -iTCP:"$API_PORT" -sTCP:LISTEN \
       > "$all_listeners" 2> "$all_listeners.stderr"; then
    return 1
  fi
  [[ ! -s "$all_listeners" && ! -s "$all_listeners.stderr" ]] || return 1
  : > "$output" || return 1
  : > "$output.stderr" || return 1
  jq -n \
    --arg captured_at_utc "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" \
    --arg label "$label" \
    --arg lsof_output "$output" \
    --argjson pid "$YAZE_PID" \
    --argjson port "$API_PORT" '
      {
        captured_at_utc: $captured_at_utc,
        launch: $label,
        role: "api",
        pid: $pid,
        port: $port,
        state: "NOT_OBSERVED",
        required_for_qualification: false,
        reason: "HTTP API is not exercised by the D6 GUI qualification",
        lsof_output: $lsof_output
      }
    ' > "$metadata" || return 1
}

wait_for_port_free "$HARNESS_PORT" ||
  die 69 "harness port is already in use: $HARNESS_PORT"
wait_for_port_free "$API_PORT" ||
  die 69 "API port is already in use: $API_PORT"

write_backup_inventory() {
  local output="$1"
  python3 - "$TARGET_BACKUP_DIR" "$output" <<'PY'
import hashlib
import json
import os
import sys

backup_dir, output = sys.argv[1:]

def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

entries = []
if os.path.isdir(backup_dir):
    for entry in sorted(os.scandir(backup_dir), key=lambda item: item.name):
        if entry.is_file(follow_symlinks=False):
            entries.append({
                "path": os.path.realpath(entry.path),
                "size": entry.stat(follow_symlinks=False).st_size,
                "sha256": sha256(entry.path),
            })

with open(output, "w", encoding="utf-8") as stream:
    json.dump({"backup_dir": backup_dir, "files": entries}, stream, indent=2)
    stream.write("\n")
PY
}

verify_backup_creation() {
  local before_inventory="$1"
  local output="$2"
  python3 - "$TARGET_BACKUP_DIR" "$before_inventory" \
    "$TARGET_ROM_SHA256_BEFORE" "$ROM" "$output" <<'PY'
import hashlib
import json
import os
import sys

backup_dir, before_path, expected_sha256, rom_path, output = sys.argv[1:]

def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

with open(before_path, encoding="utf-8") as stream:
    before = json.load(stream)
before_paths = {entry["path"] for entry in before.get("files", [])}

current = []
if os.path.isdir(backup_dir):
    for entry in sorted(os.scandir(backup_dir), key=lambda item: item.name):
        if not entry.is_file(follow_symlinks=False):
            continue
        path = os.path.realpath(entry.path)
        if path in before_paths:
            continue
        if os.path.samefile(path, rom_path):
            raise SystemExit("new backup aliases the editable ROM")
        current.append({
            "path": path,
            "size": entry.stat(follow_symlinks=False).st_size,
            "sha256": sha256(path),
        })

matches = [entry for entry in current if entry["sha256"] == expected_sha256]
result = {
    "backup_dir": backup_dir,
    "expected_pre_save_sha256": expected_sha256,
    "new_backups": current,
    "matching_pre_save_backups": matches,
}
with open(output, "w", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")

if not current:
    raise SystemExit("Save ROM did not create a new backup file")
if not matches:
    raise SystemExit("no new backup matches the pre-save ROM SHA-256")
PY
}

generate_bounded_rom_diff() {
  local before_rom="$1"
  local after_rom="$2"
  local manifest="$3"
  local object_inventory="$4"
  local sprite_inventory="$5"
  local expected_before_sha256="$6"
  local expected_after_sha256="$7"
  local output="$8"
  python3 - "$before_rom" "$after_rom" "$manifest" "$object_inventory" \
    "$sprite_inventory" "$expected_before_sha256" "$expected_after_sha256" \
    "$output" <<'PY'
import hashlib
import json
import os
import sys

(
    before_path,
    after_path,
    manifest_path,
    object_inventory_path,
    sprite_inventory_path,
    expected_before_sha256,
    expected_after_sha256,
    output,
) = sys.argv[1:]
before = open(before_path, "rb").read()
after = open(after_path, "rb").read()
manifest_bytes = open(manifest_path, "rb").read()
manifest = json.loads(manifest_bytes)
object_inventory_bytes = open(object_inventory_path, "rb").read()
sprite_inventory_bytes = open(sprite_inventory_path, "rb").read()
object_inventory = json.loads(object_inventory_bytes)
sprite_inventory = json.loads(sprite_inventory_bytes)

max_reported_ranges = 4096
max_differing_bytes = 256 * 1024
object_rooms = (0xA8, 0xB8)
sprite_rooms = (0xD8,)
door_pointer_table_pc = 0x0F83C0
palette_color_pc = 0x0DDC2E

def digest(data):
    return hashlib.sha256(data).hexdigest()

def parse_address(value, field):
    if isinstance(value, int):
        address = value
    elif isinstance(value, str):
        try:
            address = int(value, 0)
        except ValueError as error:
            raise SystemExit(f"invalid manifest address {field}={value!r}") from error
    else:
        raise SystemExit(f"manifest address {field} must be an integer or string")
    if address < 0 or address > 0xFFFFFF:
        raise SystemExit(f"manifest address {field} is outside SNES address space")
    return address

def snes_to_pc(address, field):
    bank = (address >> 16) & 0xFF
    word = address & 0xFFFF
    if word < 0x8000:
        raise SystemExit(
            f"manifest address {field}=0x{address:06X} is not LoROM-mapped"
        )
    if bank in {0x7E, 0x7F, 0xFE, 0xFF}:
        raise SystemExit(f"manifest address {field} uses a WRAM/mirror bank")
    return ((bank & 0x7F) * 0x8000) + (word & 0x7FFF)

def parse_pc(value, field):
    return snes_to_pc(parse_address(value, field), field)

def pointer_width(stream, name):
    encoding = stream.get("pointer_encoding")
    widths = {"long24": 3, "bank16": 2, "fixed_bank16": 2}
    if encoding not in widths:
        raise SystemExit(
            f"unsupported dungeon_stream_regions.{name}.pointer_encoding={encoding!r}"
        )
    return widths[encoding]

def stream_contract(name, expected_strategy):
    streams = manifest.get("dungeon_stream_regions")
    if not isinstance(streams, dict) or not isinstance(streams.get(name), dict):
        raise SystemExit(f"manifest is missing dungeon_stream_regions.{name}")
    stream = streams[name]
    if stream.get("strategy") != expected_strategy:
        raise SystemExit(
            f"dungeon_stream_regions.{name}.strategy must be {expected_strategy}"
        )
    count = stream.get("pointer_count")
    if not isinstance(count, int) or count <= 0:
        raise SystemExit(f"dungeon_stream_regions.{name}.pointer_count is invalid")
    table_pc = parse_pc(stream.get("pointer_table"), f"{name}.pointer_table")
    width = pointer_width(stream, name)

    def read_ranges(key):
        values = stream.get(key)
        if not isinstance(values, list) or not values:
            raise SystemExit(f"dungeon_stream_regions.{name}.{key} must be non-empty")
        parsed = []
        for index, value in enumerate(values):
            if not isinstance(value, dict):
                raise SystemExit(f"{name}.{key}[{index}] must be an object")
            start = parse_pc(value.get("start"), f"{name}.{key}[{index}].start")
            end = parse_pc(value.get("end"), f"{name}.{key}[{index}].end")
            if start >= end:
                raise SystemExit(f"{name}.{key}[{index}] is empty or reversed")
            parsed.append((start, end, value.get("start"), value.get("end")))
        return parsed

    data_ranges = read_ranges("data_regions")
    allocation_ranges = read_ranges("allocation_regions")
    for allocation in allocation_ranges:
        if not any(
            data_start <= allocation[0] and allocation[1] <= data_end
            for data_start, data_end, _, _ in data_ranges
        ):
            raise SystemExit(
                f"{name} allocation range 0x{allocation[0]:06X}-"
                f"0x{allocation[1]:06X} is outside declared data regions"
            )
    return {
        "name": name,
        "stream": stream,
        "pointer_count": count,
        "pointer_table_pc": table_pc,
        "pointer_width": width,
        "allocation_ranges": allocation_ranges,
    }

if len(before) != len(after):
    result = {
        "before_path": before_path,
        "after_path": after_path,
        "before_size": len(before),
        "after_size": len(after),
        "error": "ROM size changed",
        "within_bounds": False,
    }
    with open(output, "w", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
    raise SystemExit("saved ROM size differs from its pre-save backup")

if digest(before) != expected_before_sha256:
    raise SystemExit("pre-save backup does not match qualified baseline SHA-256")

contracts = {
    "objects": stream_contract("objects", "copy_on_write"),
    "sprites": stream_contract("sprites", "copy_on_write"),
    "pot_items": stream_contract("pot_items", "repack_all"),
}

def parse_inventory_pc(value, field):
    if not isinstance(value, str):
        raise SystemExit(f"stream inventory {field} must be a hexadecimal string")
    try:
        address = int(value, 0)
    except ValueError as error:
        raise SystemExit(
            f"invalid stream inventory address {field}={value!r}"
        ) from error
    if address < 0:
        raise SystemExit(f"stream inventory address {field} must not be negative")
    return address

def validate_baseline_inventory(
    inventory,
    inventory_path,
    inventory_bytes,
    name,
    rooms,
):
    contract = contracts[name]
    expected_crc32 = object_inventory.get("source_crc32")
    if (
        not isinstance(expected_crc32, str)
        or sprite_inventory.get("source_crc32") != expected_crc32
    ):
        raise SystemExit("pre-save object/sprite inventory CRC32 mismatch")
    if inventory.get("status") != "ok" or inventory.get("kind") != name:
        raise SystemExit(f"pre-save {name} stream inventory status/kind mismatch")
    if inventory.get("strategy") != "copy_on_write":
        raise SystemExit(f"pre-save {name} stream inventory strategy mismatch")
    if os.path.realpath(inventory.get("manifest", "")) != os.path.realpath(
        manifest_path
    ):
        raise SystemExit(f"pre-save {name} stream inventory manifest mismatch")
    if inventory.get("source_size") != len(before):
        raise SystemExit(f"pre-save {name} stream inventory ROM size mismatch")
    if inventory.get("source_crc32") != expected_crc32:
        raise SystemExit(f"pre-save {name} stream inventory ROM CRC32 mismatch")
    if inventory.get("pointer_encoding") != contract["stream"].get(
        "pointer_encoding"
    ):
        raise SystemExit(f"pre-save {name} stream inventory encoding mismatch")
    if inventory.get("pointer_count") != contract["pointer_count"]:
        raise SystemExit(f"pre-save {name} stream inventory pointer count mismatch")
    inventory_table_pc = parse_inventory_pc(
        inventory.get("pointer_table_pc"), f"{name}.pointer_table_pc"
    )
    if inventory_table_pc != contract["pointer_table_pc"]:
        raise SystemExit(f"pre-save {name} stream inventory pointer table mismatch")
    if inventory.get("issue_count") != 0 or inventory.get("issues") != []:
        raise SystemExit(f"pre-save {name} stream inventory reports issues")
    if (
        inventory.get("suffix_overlap_count") != 0
        or inventory.get("interior_overlap_count") != 0
        or inventory.get("overlaps") != []
    ):
        raise SystemExit(f"pre-save {name} stream inventory reports overlaps")

    unique_streams = inventory.get("unique_streams")
    exact_aliases = inventory.get("exact_aliases")
    if not isinstance(unique_streams, list) or not isinstance(exact_aliases, list):
        raise SystemExit(f"pre-save {name} stream inventory lists are invalid")

    decisions = []
    for room in rooms:
        room_id = f"0x{room:03X}"
        matches = [
            entry
            for entry in unique_streams
            if isinstance(entry, dict)
            and isinstance(entry.get("room_ids"), list)
            and room_id in entry["room_ids"]
        ]
        if len(matches) != 1:
            raise SystemExit(
                f"pre-save {name} room {room_id} must have one unique stream"
            )
        entry = matches[0]
        if entry.get("room_ids") != [room_id]:
            raise SystemExit(
                f"pre-save {name} room {room_id} stream is not uniquely owned"
            )
        if any(
            isinstance(alias, dict)
            and room_id in alias.get("room_ids", [])
            for alias in exact_aliases
        ):
            raise SystemExit(f"pre-save {name} room {room_id} is exactly aliased")
        start = parse_inventory_pc(entry.get("data_pc"), f"{name}.{room_id}.data_pc")
        end = parse_inventory_pc(entry.get("end_pc"), f"{name}.{room_id}.end_pc")
        if not (0 <= start < end <= len(before)):
            raise SystemExit(f"pre-save {name} room {room_id} span is outside ROM")
        if entry.get("bytes") != end - start:
            raise SystemExit(f"pre-save {name} room {room_id} byte count mismatch")
        decisions.append(
            {
                "kind": name,
                "room_id": room_id,
                "data_pc": f"0x{start:06X}",
                "end_pc_exclusive": f"0x{end:06X}",
                "length": end - start,
                "unique_owner": True,
                "exact_alias": False,
                "overlap": False,
                "in_place_allowance": not (name == "objects" and room == 0xB8),
                "relocation_allowance": (
                    "exact planned PC range 0x148000-0x148196"
                    if name == "objects" and room == 0xB8
                    else None
                ),
                "inventory_path": inventory_path,
                "inventory_sha256": digest(inventory_bytes),
                "_start": start,
                "_end": end,
            }
        )
    return decisions

baseline_stream_decisions = (
    validate_baseline_inventory(
        object_inventory,
        object_inventory_path,
        object_inventory_bytes,
        "objects",
        object_rooms,
    )
    + validate_baseline_inventory(
        sprite_inventory,
        sprite_inventory_path,
        sprite_inventory_bytes,
        "sprites",
        sprite_rooms,
    )
)

allowed = []

def add_allowed(start, end, reason, source, proof=None):
    if start < 0 or start >= end or end > len(before):
        raise SystemExit(
            f"allowlist range 0x{start:06X}-0x{end:06X} is outside the ROM"
        )
    rule = {
        "start": start,
        "end": end,
        "start_pc": f"0x{start:06X}",
        "end_pc_exclusive": f"0x{end:06X}",
        "length": end - start,
        "reason": reason,
        "source": source,
    }
    if proof is not None:
        rule["proof"] = proof
    allowed.append(rule)

objects = contracts["objects"]
if objects["pointer_width"] != 3:
    raise SystemExit("objects copy-on-write qualification requires long24 pointers")
sprites = contracts["sprites"]
if sprites["pointer_width"] != 2:
    raise SystemExit("sprite copy-on-write qualification requires bank16 pointers")
pots = contracts["pot_items"]

decision_by_room = {
    (decision["kind"], decision["room_id"]): decision
    for decision in baseline_stream_decisions
}
expected_baseline_spans = {
    ("objects", "0x0A8"): (0x0FC169, 0x0FC272),
    ("objects", "0x0B8"): (0x13861D, 0x1387B1),
    ("sprites", "0x0D8"): (0x04E59F, 0x04E5B6),
}
for key, expected_span in expected_baseline_spans.items():
    decision = decision_by_room.get(key)
    if decision is None or (decision["_start"], decision["_end"]) != expected_span:
        raise SystemExit(f"qualified baseline stream span changed for {key}")

a8_stream = expected_baseline_spans[("objects", "0x0A8")]
b8_stream = expected_baseline_spans[("objects", "0x0B8")]
d8_sprite_stream = expected_baseline_spans[("sprites", "0x0D8")]
a8_door_type_pc = 0x0FC26F
d8_sprite_x_pc = 0x04E5A7
d8_pot_item_pc = 0x00E512
b8_cow_payload = (0x148000, 0x148196)
b8_main_pointer = (
    objects["pointer_table_pc"] + 0xB8 * objects["pointer_width"],
    objects["pointer_table_pc"] + 0xB8 * objects["pointer_width"] + 3,
)
b8_door_pointer = (
    door_pointer_table_pc + 0xB8 * 3,
    door_pointer_table_pc + 0xB8 * 3 + 3,
)
a8_main_pointer = (
    objects["pointer_table_pc"] + 0xA8 * objects["pointer_width"],
    objects["pointer_table_pc"] + 0xA8 * objects["pointer_width"] + 3,
)
a8_door_pointer = (
    door_pointer_table_pc + 0xA8 * 3,
    door_pointer_table_pc + 0xA8 * 3 + 3,
)
d8_sprite_pointer = (
    sprites["pointer_table_pc"] + 0xD8 * sprites["pointer_width"],
    sprites["pointer_table_pc"] + 0xD8 * sprites["pointer_width"] + 2,
)
if not (a8_stream[0] <= a8_door_type_pc < a8_stream[1]):
    raise SystemExit("A8 door type byte is outside its inventoried stream")
if not (d8_sprite_stream[0] <= d8_sprite_x_pc < d8_sprite_stream[1]):
    raise SystemExit("D8 sprite X byte is outside its inventoried stream")
if not any(
    start <= b8_cow_payload[0]
    and b8_cow_payload[1] <= end
    for start, end, _, _ in objects["allocation_ranges"]
):
    raise SystemExit("planned B8 COW payload is outside object allocation")
if not any(
    start <= d8_pot_item_pc < end
    for start, end, _, _ in pots["allocation_ranges"]
):
    raise SystemExit("planned D8 pot byte is outside pot allocation")

add_allowed(
    d8_pot_item_pc,
    d8_pot_item_pc + 1,
    "D8 deterministic pot repack item byte",
    "pot repack golden constrained by full post-save SHA-256",
)
add_allowed(
    d8_sprite_x_pc,
    d8_sprite_x_pc + 1,
    "D8 sprite[2] X byte in exact inventoried stream",
    "pre-save z3ed dungeon-stream-plan",
)
add_allowed(
    palette_color_pc,
    palette_color_pc + 2,
    "room 0x0DA palette 7 color 7 BGR555 word",
    "z3ed baseline color_pc 0x0DDC2E",
)
add_allowed(
    *b8_main_pointer,
    "B8 object COW main-pointer slot",
    "dungeon_stream_regions.objects.pointer_table",
)
add_allowed(
    *b8_door_pointer,
    "B8 object COW auxiliary door-pointer slot",
    "zelda3::kDoorPointers PC 0x0F83C0",
)
add_allowed(
    a8_door_type_pc,
    a8_door_type_pc + 1,
    "A8 door[2] raw type byte in exact inventoried stream",
    "pre-save z3ed dungeon-stream-plan",
)
add_allowed(
    *b8_cow_payload,
    "B8 exact planned copy-on-write payload",
    "manifest allocation first-fit plus 404-byte baseline and F0 FF terminator",
)

allowed.sort(key=lambda item: (item["start"], item["end"], item["reason"]))
merged_allowed = []
for rule in allowed:
    if not merged_allowed or rule["start"] > merged_allowed[-1][1]:
        merged_allowed.append([rule["start"], rule["end"]])
    else:
        merged_allowed[-1][1] = max(merged_allowed[-1][1], rule["end"])

ranges = []
start = None
differing_bytes = 0
for offset, (old, new) in enumerate(zip(before, after)):
    if old != new:
        differing_bytes += 1
        if start is None:
            start = offset
    elif start is not None:
        ranges.append((start, offset))
        start = None
if start is not None:
    ranges.append((start, len(before)))

outside = []
for changed_start, changed_end in ranges:
    cursor = changed_start
    for allowed_start, allowed_end in merged_allowed:
        if allowed_end <= cursor:
            continue
        if allowed_start >= changed_end:
            break
        if allowed_start > cursor:
            outside.append((cursor, min(allowed_start, changed_end)))
        cursor = max(cursor, allowed_end)
        if cursor >= changed_end:
            break
    if cursor < changed_end:
        outside.append((cursor, changed_end))

expected_changed_ranges = [
    (0x00E512, 0x00E513),
    (0x04E5A7, 0x04E5A8),
    (0x0DDC2E, 0x0DDC2F),
    (0x0F8228, 0x0F822B),
    (0x0F85E8, 0x0F85EB),
    (0x0FC26F, 0x0FC270),
    (0x148000, 0x148196),
]
expected_transitions = [
    (0x00E512, bytes.fromhex("0A"), bytes.fromhex("09"), "D8 pot item"),
    (0x04E5A7, bytes.fromhex("04"), bytes.fromhex("05"), "D8 sprite X"),
    (0x0DDC2E, bytes.fromhex("FF"), bytes.fromhex("FE"), "DA palette"),
    (
        b8_main_pointer[0],
        bytes.fromhex("1D86A7"),
        bytes.fromhex("008029"),
        "B8 object pointer",
    ),
    (
        b8_door_pointer[0],
        bytes.fromhex("1F86A7"),
        bytes.fromhex("948129"),
        "B8 door pointer",
    ),
    (0x0FC26F, bytes.fromhex("18"), bytes.fromhex("00"), "A8 door type"),
]
golden_errors = []
if digest(after) != expected_after_sha256:
    golden_errors.append("post-save SHA-256 mismatch")
if differing_bytes != 416:
    golden_errors.append(f"expected 416 changed bytes, found {differing_bytes}")
if ranges != expected_changed_ranges:
    golden_errors.append("changed ranges do not match the seven-range golden")
for offset, old, new, label in expected_transitions:
    if before[offset : offset + len(old)] != old:
        golden_errors.append(f"{label} baseline bytes mismatch")
    if after[offset : offset + len(new)] != new:
        golden_errors.append(f"{label} saved bytes mismatch")
if before[b8_stream[0] : b8_stream[1]] != after[b8_stream[0] : b8_stream[1]]:
    golden_errors.append("old B8 object stream changed during COW")
for label, span in (
    ("A8 object pointer", a8_main_pointer),
    ("A8 door pointer", a8_door_pointer),
    ("D8 sprite pointer", d8_sprite_pointer),
):
    if before[span[0] : span[1]] != after[span[0] : span[1]]:
        golden_errors.append(f"{label} unexpectedly changed")
pot_pointer_span = (
    pots["pointer_table_pc"],
    pots["pointer_table_pc"] + pots["pointer_count"] * pots["pointer_width"],
)
if before[pot_pointer_span[0] : pot_pointer_span[1]] != after[
    pot_pointer_span[0] : pot_pointer_span[1]
]:
    golden_errors.append("pot repack pointer table unexpectedly changed")
golden_matches = not golden_errors

reported = []
for start, end in ranges[:max_reported_ranges]:
    old = before[start:end]
    new = after[start:end]
    matching_rules = [
        rule["reason"]
        for rule in allowed
        if rule["start"] < end and start < rule["end"]
    ]
    reported.append({
        "start_pc": f"0x{start:06X}",
        "end_pc_exclusive": f"0x{end:06X}",
        "length": end - start,
        "before_sha256": digest(old),
        "after_sha256": digest(new),
        "before_prefix_hex": old[:16].hex().upper(),
        "after_prefix_hex": new[:16].hex().upper(),
        "allowed_by": matching_rules,
    })

all_changed_bytes_contained = not outside
magnitude_within_bounds = (
    differing_bytes > 0
    and differing_bytes <= max_differing_bytes
    and len(ranges) <= max_reported_ranges
)
within_bounds = (
    magnitude_within_bounds and all_changed_bytes_contained and golden_matches
)
result = {
    "before_path": before_path,
    "after_path": after_path,
    "rom_size": len(before),
    "before_sha256": digest(before),
    "after_sha256": digest(after),
    "differing_byte_count": differing_bytes,
    "range_count": len(ranges),
    "limits": {
        "max_differing_bytes": max_differing_bytes,
        "max_reported_ranges": max_reported_ranges,
    },
    "ranges_truncated": len(ranges) > max_reported_ranges,
    "magnitude_within_bounds": magnitude_within_bounds,
    "golden_matches": golden_matches,
    "within_bounds": within_bounds,
    "allowlist": {
        "source_manifest": manifest_path,
        "source_manifest_sha256": digest(manifest_bytes),
        "baseline_stream_inventories": {
            "objects": {
                "path": object_inventory_path,
                "sha256": digest(object_inventory_bytes),
                "source_crc32": object_inventory.get("source_crc32"),
            },
            "sprites": {
                "path": sprite_inventory_path,
                "sha256": digest(sprite_inventory_bytes),
                "source_crc32": sprite_inventory.get("source_crc32"),
            },
        },
        "address_conversion": "LoROM SNES address to headerless PC offset",
        "edited_object_rooms": [f"0x{room:03X}" for room in object_rooms],
        "edited_sprite_rooms": [f"0x{room:03X}" for room in sprite_rooms],
        "baseline_stream_decisions": [
            {
                key: value
                for key, value in decision.items()
                if key not in {"_start", "_end"}
            }
            for decision in baseline_stream_decisions
        ],
        "rules": [
            {key: value for key, value in rule.items() if key not in {"start", "end"}}
            for rule in allowed
        ],
        "merged_pc_ranges": [
            {
                "start_pc": f"0x{start:06X}",
                "end_pc_exclusive": f"0x{end:06X}",
                "length": end - start,
            }
            for start, end in merged_allowed
        ],
    },
    "allowlist_proof": {
        "all_changed_bytes_contained": all_changed_bytes_contained,
        "outside_range_count": len(outside),
        "outside_ranges_truncated": len(outside) > max_reported_ranges,
        "outside_ranges": [
            {
                "start_pc": f"0x{start:06X}",
                "end_pc_exclusive": f"0x{end:06X}",
                "length": end - start,
            }
            for start, end in outside[:max_reported_ranges]
        ],
    },
    "golden_proof": {
        "expected_before_sha256": expected_before_sha256,
        "expected_after_sha256": expected_after_sha256,
        "expected_differing_byte_count": 416,
        "expected_range_count": 7,
        "expected_ranges": [
            {
                "start_pc": f"0x{start:06X}",
                "end_pc_exclusive": f"0x{end:06X}",
                "length": end - start,
            }
            for start, end in expected_changed_ranges
        ],
        "old_b8_stream_unchanged": (
            before[b8_stream[0] : b8_stream[1]]
            == after[b8_stream[0] : b8_stream[1]]
        ),
        "a8_main_pointer_unchanged": (
            before[a8_main_pointer[0] : a8_main_pointer[1]]
            == after[a8_main_pointer[0] : a8_main_pointer[1]]
        ),
        "a8_door_pointer_unchanged": (
            before[a8_door_pointer[0] : a8_door_pointer[1]]
            == after[a8_door_pointer[0] : a8_door_pointer[1]]
        ),
        "d8_sprite_pointer_unchanged": (
            before[d8_sprite_pointer[0] : d8_sprite_pointer[1]]
            == after[d8_sprite_pointer[0] : d8_sprite_pointer[1]]
        ),
        "pot_pointer_table_unchanged": (
            before[pot_pointer_span[0] : pot_pointer_span[1]]
            == after[pot_pointer_span[0] : pot_pointer_span[1]]
        ),
        "errors": golden_errors,
    },
    "ranges": reported,
}
with open(output, "w", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")

if not within_bounds:
    raise SystemExit(
        "ROM diff rejected: "
        f"bytes={differing_bytes}, ranges={len(ranges)}, "
        f"outside_allowlist_ranges={len(outside)}, "
        f"golden_errors={len(golden_errors)}"
    )
PY
}

capture_readback() {
  local phase="$1"
  local door_type="$2"
  local object_y="$3"
  local sprite_x="$4"
  local item_id="$5"
  local palette_color="$6"
  local out="$EVIDENCE_DIR/$phase"
  mkdir "$out"

  NO_COLOR=1 TERM=dumb "$Z3ED_BIN" dungeon-describe-room \
    --room=0xA8 --rom="$ROM" --format=json > "$out/a8-door.json" ||
    die 70 "$phase z3ed door readback failed"
  NO_COLOR=1 TERM=dumb "$Z3ED_BIN" dungeon-list-objects \
    --room=0xB8 --rom="$ROM" --format=json > "$out/b8-object.json" ||
    die 70 "$phase z3ed object readback failed"
  NO_COLOR=1 TERM=dumb "$Z3ED_BIN" dungeon-list-sprites \
    --room=0xD8 --rom="$ROM" --format=json > "$out/d8-sprites.json" ||
    die 70 "$phase z3ed sprite readback failed"
  NO_COLOR=1 TERM=dumb "$Z3ED_BIN" dungeon-list-pot-items \
    --room=0xD8 --rom="$ROM" --format=json > "$out/d8-pot-items.json" ||
    die 70 "$phase z3ed pot-item readback failed"
  NO_COLOR=1 TERM=dumb "$Z3ED_BIN" dungeon-get-palette \
    --room=0xDA --index=7 --rom="$ROM" --format=json > "$out/da-palette.json" ||
    die 70 "$phase z3ed palette readback failed"

  jq -e --arg expected "$door_type" '
    .doors[2] as $door |
    $door.index == 2 and $door.tile_x == 46 and $door.tile_y == 58 and
    $door.type_id == $expected
  ' "$out/a8-door.json" >/dev/null ||
    die 74 "$phase A8 door[2] readback mismatch"
  jq -e --argjson expected "$object_y" '
    .["Dungeon Room Objects"].objects[128] as $object |
    $object.id_hex == "0x00BC" and $object.x == 28 and
    $object.size == 0 and $object.layer == 0 and $object.y == $expected
  ' "$out/b8-object.json" >/dev/null ||
    die 74 "$phase B8 object[128] readback mismatch"
  jq -e --argjson expected "$sprite_x" '
    .["Dungeon Room Sprites"].sprites[2] as $sprite |
    $sprite.sprite_id == "0x64" and $sprite.y == 5 and
    $sprite.subtype == 0 and $sprite.layer == 0 and $sprite.x == $expected
  ' "$out/d8-sprites.json" >/dev/null ||
    die 74 "$phase D8 sprite[2] readback mismatch"
  jq -e --arg expected "$item_id" '
    .["Dungeon Pot Items"].items[5] as $item |
    $item.position == "0x0C4C" and $item.tile_x == 38 and
    $item.tile_y == 24 and $item.item_id == $expected
  ' "$out/d8-pot-items.json" >/dev/null ||
    die 74 "$phase D8 pot[5] readback mismatch"
  jq -e --arg expected "$palette_color" '
    .["Dungeon Palette"] as $palette |
    $palette.room_id == "0x0DA" and
    $palette.palette_set_id == "0x07" and
    $palette.resolved_palette_index == 7 and
    $palette.color_index == 7 and
    $palette.color_pc == "0x0DDC2E" and
    $palette.color_snes == $expected
  ' "$out/da-palette.json" >/dev/null ||
    die 74 "$phase DA palette 7 color 7 readback mismatch"

  {
    echo "phase=$phase"
    echo "captured_at_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    echo "rom_sha1=$(sha1_file "$ROM")"
    echo "rom_sha256=$(sha256_file "$ROM")"
    echo "expected_door_type=$door_type"
    echo "expected_object_y=$object_y"
    echo "expected_sprite_x=$sprite_x"
    echo "expected_item_id=$item_id"
    echo "expected_palette_color=$palette_color"
  } > "$out/readback.txt"
}

wait_for_harness() {
  local label="$1"
  local deadline=$((SECONDS + 30))
  local log="$EVIDENCE_DIR/$label-harness-ready.txt"
  while [[ "$SECONDS" -lt "$deadline" ]]; do
    if ! kill -0 "$YAZE_PID" 2>/dev/null; then
      return 1
    fi
    if NO_COLOR=1 TERM=dumb "$Z3ED_BIN" agent test list \
      --host 127.0.0.1 --port "$HARNESS_PORT" > "$log" 2>&1; then
      return 0
    fi
    sleep 0.25
  done
  return 1
}

wait_for_room_widget() {
  local label="$1"
  local key="Dungeon/Workbench/input_scalar:room_id"
  local deadline=$((SECONDS + 30))
  local output="$EVIDENCE_DIR/$label-room-widget.json"
  while [[ "$SECONDS" -lt "$deadline" ]]; do
    if ! kill -0 "$YAZE_PID" 2>/dev/null; then
      return 1
    fi
    if NO_COLOR=1 TERM=dumb "$Z3ED_BIN" gui-assert \
      --widget-key="$key" \
      --gui_server_address="127.0.0.1:$HARNESS_PORT" \
      --format=json > "$output" 2>/dev/null &&
       jq -e --arg key "$key" '
         .["GUI Assert Action"].status == "Success" and
         .["GUI Assert Action"].resolved_widget_key == $key
       ' "$output" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.25
  done
  return 1
}

launch_yaze() {
  local label="$1"
  local app_data="$EVIDENCE_DIR/$label-yaze-app-data"
  local runtime_dir="$EVIDENCE_DIR/$label-runtime"
  local log_file="$EVIDENCE_DIR/$label-yaze.log"
  mkdir -p "$app_data" "$runtime_dir"
  chmod 700 "$runtime_dir"

  local command=(
    "$YAZE_BIN"
    --service
    --enable_test_harness
    "--test_harness_port=$HARNESS_PORT"
    "--api_port=$API_PORT"
    "--rom_file=$PROJECT"
    --editor=Dungeon
    --open_panels=dungeon.workbench
    --startup_welcome=hide
    --startup_dashboard=hide
    --startup_sidebar=hide
    "--log_file=$log_file"
    --log_to_console
  )

  {
    printf 'YAZE_APP_DATA_DIR=%q XDG_RUNTIME_DIR=%q ' "$app_data" "$runtime_dir"
    printf '%q ' "${command[@]}"
    printf '\n'
  } > "$EVIDENCE_DIR/$label-command.txt"

  YAZE_APP_DATA_DIR="$app_data" XDG_RUNTIME_DIR="$runtime_dir" \
    "${command[@]}" > "$EVIDENCE_DIR/$label-stdout-stderr.txt" 2>&1 &
  YAZE_PID=$!
  echo "$YAZE_PID" > "$EVIDENCE_DIR/$label.pid"

  wait_for_owned_listener "$label" harness "$HARNESS_PORT" ||
    die 70 "$label harness listener is not owned by Yaze PID $YAZE_PID"
  record_optional_api_listener "$label" ||
    die 70 "$label optional API listener could not be classified safely"
  wait_for_harness "$label" ||
    die 70 "$label Yaze process did not expose the gRPC harness; see $log_file"
  wait_for_room_widget "$label" ||
    die 70 "$label Yaze process did not render the Workbench room selector"
}

run_replay() {
  local fixture="$1"
  local label="$2"
  NO_COLOR=1 TERM=dumb "$Z3ED_BIN" agent test replay "$fixture" \
    --host 127.0.0.1 --port "$HARNESS_PORT" --ci \
    > "$EVIDENCE_DIR/$label-replay.txt" 2>&1
}

wait_for_process_exit() {
  local label="$1"
  local deadline=$((SECONDS + 20))
  while kill -0 "$YAZE_PID" 2>/dev/null && [[ "$SECONDS" -lt "$deadline" ]]; do
    sleep 0.1
  done
  if kill -0 "$YAZE_PID" 2>/dev/null; then
    die 70 "$label Yaze process did not exit after the registered Quit action"
  fi
  local exited_pid="$YAZE_PID"
  local process_status=0
  wait "$exited_pid" || process_status=$?
  YAZE_PID=""
  [[ "$process_status" -eq 0 ]] ||
    die 70 "$label Yaze process exited with status $process_status"
}

cat > "$EVIDENCE_DIR/quit.json" <<'EOF_QUIT'
{
  "schema_version": 1,
  "name": "Quit the D6 qualification instance",
  "steps": [
    {
      "action": "click",
      "target": "menu:File",
      "expect": { "success": true, "status": "passed" }
    },
    {
      "action": "click",
      "widget_key": "menuitem:Quit",
      "expect": { "success": true, "status": "passed" }
    }
  ]
}
EOF_QUIT

BACKUP_INVENTORY_BEFORE="$EVIDENCE_DIR/backups-before.json"
BACKUP_VERIFICATION="$EVIDENCE_DIR/backups-after-save.json"
write_backup_inventory "$BACKUP_INVENTORY_BEFORE" ||
  die 70 "could not inventory the disposable ROM backup directory"

capture_readback baseline 0x18 7 4 0x0A 0x7FFF
assert_frozen_inputs_unchanged || die 74 "a frozen input changed during baseline"

launch_yaze first-launch
FIRST_PID="$YAZE_PID"
run_replay "$FROZEN_EDIT_FIXTURE" edit-and-save
verify_backup_creation "$BACKUP_INVENTORY_BEFORE" "$BACKUP_VERIFICATION" ||
  die 74 "Save ROM backup verification failed; see $BACKUP_VERIFICATION"
BACKUP_MATCH_PATH="$(jq -er '.matching_pre_save_backups[0].path' \
  "$BACKUP_VERIFICATION")" ||
  die 74 "backup verification did not record a matching backup path"
BACKUP_MATCH_SHA256="$(jq -er '.matching_pre_save_backups[0].sha256' \
  "$BACKUP_VERIFICATION")" ||
  die 74 "backup verification did not record a matching backup hash"
NEW_BACKUP_COUNT="$(jq -er '.new_backups | length' "$BACKUP_VERIFICATION")" ||
  die 74 "backup verification did not record new backup files"

TARGET_ROM_SHA256_AFTER_SAVE="$(sha256_file "$ROM")"
[[ "$TARGET_ROM_SHA256_AFTER_SAVE" != "$TARGET_ROM_SHA256_BEFORE" ]] ||
  die 74 "Save ROM reported success but the disposable ROM hash did not change"
ROM_BYTE_DIFF="$EVIDENCE_DIR/rom-byte-diff.json"
generate_bounded_rom_diff \
  "$BACKUP_MATCH_PATH" "$ROM" "$TARGET_MANIFEST" \
  "$OBJECT_STREAM_INVENTORY" "$SPRITE_STREAM_INVENTORY" \
  "$QUALIFIED_BASELINE_SHA256" "$QUALIFIED_SAVED_SHA256" \
  "$ROM_BYTE_DIFF" ||
  die 74 "saved ROM byte diff exceeded bounds or allowlist; see $ROM_BYTE_DIFF"
ROM_DIFF_BYTE_COUNT="$(jq -er '.differing_byte_count' "$ROM_BYTE_DIFF")" ||
  die 74 "ROM byte diff did not record a differing-byte count"
ROM_DIFF_RANGE_COUNT="$(jq -er '.range_count' "$ROM_BYTE_DIFF")" ||
  die 74 "ROM byte diff did not record a range count"
ROM_DIFF_ALLOWLIST_RULE_COUNT="$(jq -er '.allowlist.rules | length' \
  "$ROM_BYTE_DIFF")" || die 74 "ROM byte diff did not record allowlist rules"
ROM_DIFF_OUTSIDE_RANGE_COUNT="$(jq -er \
  '.allowlist_proof.outside_range_count' "$ROM_BYTE_DIFF")" ||
  die 74 "ROM byte diff did not record outside-allowlist ranges"
ROM_DIFF_BASELINE_STREAM_COUNT="$(jq -er \
  '.allowlist.baseline_stream_decisions | length' "$ROM_BYTE_DIFF")" ||
  die 74 "ROM byte diff did not record baseline stream decisions"
jq -e '
  .within_bounds == true and
  .golden_matches == true and
  .golden_proof.expected_differing_byte_count == 416 and
  .golden_proof.expected_range_count == 7 and
  .golden_proof.old_b8_stream_unchanged == true and
  .golden_proof.a8_main_pointer_unchanged == true and
  .golden_proof.a8_door_pointer_unchanged == true and
  .golden_proof.d8_sprite_pointer_unchanged == true and
  .golden_proof.pot_pointer_table_unchanged == true and
  (.golden_proof.errors | length) == 0 and
  .allowlist_proof.all_changed_bytes_contained == true and
  .allowlist_proof.outside_range_count == 0 and
  (.allowlist.baseline_stream_decisions | length) == 3 and
  ([.allowlist.baseline_stream_decisions[] |
      select(.unique_owner == true and .exact_alias == false and
             .overlap == false)] | length) == 3
' "$ROM_BYTE_DIFF" >/dev/null ||
  die 74 "ROM byte diff containment proof failed; see $ROM_BYTE_DIFF"
capture_readback before-quit 0x00 8 5 0x09 0x7FFE
assert_frozen_inputs_unchanged || die 74 "a frozen input changed before first quit"

run_replay "$EVIDENCE_DIR/quit.json" first-quit
wait_for_process_exit first-launch
[[ "$(sha256_file "$ROM")" == "$TARGET_ROM_SHA256_AFTER_SAVE" ]] ||
  die 74 "disposable ROM changed while quitting the first launch"

wait_for_port_free "$HARNESS_PORT" ||
  die 70 "harness port did not close after first launch"
wait_for_port_free "$API_PORT" ||
  die 70 "API port did not close after first launch"

launch_yaze second-launch
SECOND_PID="$YAZE_PID"
run_replay "$FROZEN_REOPEN_FIXTURE" reopen-assert
capture_readback after-relaunch 0x00 8 5 0x09 0x7FFE
[[ "$(sha256_file "$ROM")" == "$TARGET_ROM_SHA256_AFTER_SAVE" ]] ||
  die 74 "reopened ROM hash differs from the post-save hash"
assert_frozen_inputs_unchanged || die 74 "a frozen input changed after relaunch"

run_replay "$EVIDENCE_DIR/quit.json" second-quit
wait_for_process_exit second-launch
[[ "$(sha256_file "$ROM")" == "$TARGET_ROM_SHA256_AFTER_SAVE" ]] ||
  die 74 "disposable ROM changed while quitting the second launch"
assert_frozen_inputs_unchanged || die 74 "a frozen input changed at completion"

jq -n \
  --arg completed_at_utc "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" \
  --arg project "$PROJECT" \
  --arg rom "$ROM" \
  --arg canonical_project "$CANONICAL_PROJECT" \
  --arg canonical_rom "$CANONICAL_ROM" \
  --arg manifest "$TARGET_MANIFEST" \
  --arg canonical_manifest "$CANONICAL_MANIFEST" \
  --arg yaze_bin "$YAZE_BIN" \
  --arg z3ed_bin "$Z3ED_BIN" \
  --arg yaze_bin_sha256 "$YAZE_BIN_SHA256" \
  --arg z3ed_bin_sha256 "$Z3ED_BIN_SHA256" \
  --arg project_sha256 "$TARGET_PROJECT_SHA256_BEFORE" \
  --arg manifest_sha256 "$TARGET_MANIFEST_SHA256_BEFORE" \
  --arg canonical_project_sha256 "$CANONICAL_PROJECT_SHA256_BEFORE" \
  --arg canonical_manifest_sha256 "$CANONICAL_MANIFEST_SHA256_BEFORE" \
  --arg initial_sha256 "$TARGET_ROM_SHA256_BEFORE" \
  --arg saved_sha256 "$TARGET_ROM_SHA256_AFTER_SAVE" \
  --arg canonical_sha256 "$CANONICAL_ROM_SHA256_BEFORE" \
  --arg provenance "$PROVENANCE_EVIDENCE" \
  --arg feature_flags "$FEATURE_FLAG_EVIDENCE" \
  --arg object_stream_inventory "$OBJECT_STREAM_INVENTORY" \
  --arg object_stream_inventory_sha256 "$OBJECT_STREAM_INVENTORY_SHA256" \
  --arg sprite_stream_inventory "$SPRITE_STREAM_INVENTORY" \
  --arg sprite_stream_inventory_sha256 "$SPRITE_STREAM_INVENTORY_SHA256" \
  --arg stream_inventory_validation "$STREAM_INVENTORY_VALIDATION" \
  --arg stream_inventory_validation_sha256 "$STREAM_INVENTORY_VALIDATION_SHA256" \
  --arg frozen_edit_fixture "$FROZEN_EDIT_FIXTURE" \
  --arg frozen_reopen_fixture "$FROZEN_REOPEN_FIXTURE" \
  --arg yaze_git_head "$YAZE_GIT_HEAD" \
  --arg git_status "$GIT_STATUS_EVIDENCE" \
  --arg rom_byte_diff "$ROM_BYTE_DIFF" \
  --arg first_harness_listener "$EVIDENCE_DIR/first-launch-harness-listener.json" \
  --arg first_api_listener "$EVIDENCE_DIR/first-launch-api-listener.json" \
  --arg second_harness_listener "$EVIDENCE_DIR/second-launch-harness-listener.json" \
  --arg second_api_listener "$EVIDENCE_DIR/second-launch-api-listener.json" \
  --arg backup_dir "$TARGET_BACKUP_DIR" \
  --arg backup_path "$BACKUP_MATCH_PATH" \
  --arg backup_sha256 "$BACKUP_MATCH_SHA256" \
  --arg backup_inventory_before "$BACKUP_INVENTORY_BEFORE" \
  --arg backup_verification "$BACKUP_VERIFICATION" \
  --argjson new_backup_count "$NEW_BACKUP_COUNT" \
  --argjson yaze_git_dirty "$YAZE_GIT_DIRTY" \
  --argjson manifests_equal_before "$MANIFESTS_EQUAL_BEFORE" \
  --argjson rom_diff_byte_count "$ROM_DIFF_BYTE_COUNT" \
  --argjson rom_diff_range_count "$ROM_DIFF_RANGE_COUNT" \
  --argjson rom_diff_allowlist_rule_count "$ROM_DIFF_ALLOWLIST_RULE_COUNT" \
  --argjson rom_diff_outside_range_count "$ROM_DIFF_OUTSIDE_RANGE_COUNT" \
  --argjson rom_diff_baseline_stream_count "$ROM_DIFF_BASELINE_STREAM_COUNT" \
  --argjson first_pid "$FIRST_PID" \
  --argjson second_pid "$SECOND_PID" \
  --argjson harness_port "$HARNESS_PORT" \
  --argjson api_port "$API_PORT" '
    {
      status: "pass",
      completed_at_utc: $completed_at_utc,
      http_api_required: false,
      disposable: {
        project: {path: $project, sha256: $project_sha256},
        rom: $rom,
        manifest: {path: $manifest, sha256: $manifest_sha256},
        initial_sha256: $initial_sha256,
        saved_sha256: $saved_sha256
      },
      canonical: {
        project: {path: $canonical_project, sha256: $canonical_project_sha256},
        rom: $canonical_rom,
        rom_sha256: $canonical_sha256,
        manifest: {
          path: $canonical_manifest,
          sha256: $canonical_manifest_sha256
        },
        unchanged: true
      },
      manifests_equal_before: $manifests_equal_before,
      backup: {
        directory: $backup_dir,
        matching_pre_save_path: $backup_path,
        matching_pre_save_sha256: $backup_sha256,
        new_backup_count: $new_backup_count,
        inventory_before: $backup_inventory_before,
        verification: $backup_verification
      },
      provenance: {
        file: $provenance,
        feature_flags: $feature_flags,
        yaze_git: {
          head: $yaze_git_head,
          dirty: $yaze_git_dirty,
          status_file: $git_status
        },
        frozen_fixtures: {
          edit: $frozen_edit_fixture,
          reopen: $frozen_reopen_fixture
        },
        baseline_stream_inventory: {
          objects: {
            path: $object_stream_inventory,
            sha256: $object_stream_inventory_sha256
          },
          sprites: {
            path: $sprite_stream_inventory,
            sha256: $sprite_stream_inventory_sha256
          },
          validation: {
            path: $stream_inventory_validation,
            sha256: $stream_inventory_validation_sha256
          }
        }
      },
      binaries: {
        yaze: {path: $yaze_bin, sha256: $yaze_bin_sha256},
        z3ed: {path: $z3ed_bin, sha256: $z3ed_bin_sha256}
      },
      launches: [
        {
          label: "first-launch",
          pid: $first_pid,
          harness_listener: $first_harness_listener,
          api_listener_observation: $first_api_listener
        },
        {
          label: "second-launch",
          pid: $second_pid,
          harness_listener: $second_harness_listener,
          api_listener_observation: $second_api_listener
        }
      ],
      ports: {harness: $harness_port, api: $api_port},
      gui_replay: {
        edit_and_save: "pass",
        reopen_assert: "pass",
        registered_quit_actions: 2
      },
      z3ed_readback: {
        baseline: "pass",
        before_quit: "pass",
        after_relaunch: "pass"
      },
      rom_byte_diff: {
        file: $rom_byte_diff,
        differing_byte_count: $rom_diff_byte_count,
        range_count: $rom_diff_range_count,
        bounded: true,
        allowlist_rule_count: $rom_diff_allowlist_rule_count,
        baseline_stream_decision_count: $rom_diff_baseline_stream_count,
        all_changed_bytes_contained: true,
        outside_allowlist_range_count: $rom_diff_outside_range_count
      }
    }
  ' > "$EVIDENCE_DIR/qualification-summary.json"

echo "D6 GUI save/reopen qualification passed"
echo "  evidence: $EVIDENCE_DIR"
echo "  initial ROM SHA-256: $TARGET_ROM_SHA256_BEFORE"
echo "  saved ROM SHA-256:   $TARGET_ROM_SHA256_AFTER_SAVE"
echo "  pre-save backup:      $BACKUP_MATCH_PATH"
echo "  backup SHA-256:       $BACKUP_MATCH_SHA256"
echo "  ROM byte diff:        $ROM_BYTE_DIFF ($ROM_DIFF_BYTE_COUNT bytes, $ROM_DIFF_RANGE_COUNT ranges)"
echo "  diff containment:     $ROM_DIFF_ALLOWLIST_RULE_COUNT allowlist rules, $ROM_DIFF_BASELINE_STREAM_COUNT baseline streams, 0 outside ranges"
echo "  canonical unchanged: $CANONICAL_ROM_SHA256_BEFORE"
