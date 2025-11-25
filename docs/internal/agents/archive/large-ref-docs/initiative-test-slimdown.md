# Initiative: Test Suite Slimdown & Gating

## Goal
Reduce test bloat, keep high-signal coverage, and gate optional AI/ROM/bench suites. Deliver lean default CI (stable + smokes) with optional nightly heavy suites.

## Scope & Owners
- **test-infrastructure-expert**: Owns harness/labels/CTests; flake triage and duplication removal.
- **ai-infra-architect**: Owns AI/experimental/ROM gating logic (skip when keys/runtime missing).
- **docs-janitor**: Updates docs (test/README, CI docs) for default vs optional suites.
- **backend-infra-engineer**: CI pipeline changes (default vs nightly matrices).
- **imgui-frontend-engineer**: Rendering/UI test pruning, keep one rendering suite.
- **snes-emulator-expert**: Consult if emulator tests are affected.
- **GEMINI_AUTOM**: Quick TODO fixes in tests (small, low-risk).

## Deliverables
1) Default test set: stable + e2e smokes (framework, dungeon editor, canvas); one rendering suite only.
2) Optional suites gated: ROM-dependent, AI experimental, benchmarks (off by default); skip cleanly when missing ROM/keys.
3) Prune duplicates: drop legacy rendering/e2e duplicates and legacy dungeon_editor_test if v2 covers it.
4) Docs: Updated test/README and CI docs with clear run commands and labels.
5) CI: PR/commit matrix runs lean set; nightly matrix runs optional suites.

## Tasks
- Inventory and prune
  - Keep integration/dungeon_object_rendering_tests_new.cc; drop older rendering integration + e2e variants.
  - Drop/retire dungeon_editor_test.cc (v1) if v2 covers current UI.
- Gating
  - Ensure yaze_test_experimental and rom_dependent suites are off by default; add labels/presets for nightly.
  - AI tests skip gracefully if AI runtime/key missing.
- CI changes
  - PR: stable + smokes only; Nightly: add ROM + AI + bench.
- Docs
  - Update test/README.md and CI docs to reflect default vs optional suites and commands/labels.
- Quick fixes
  - Triage TODOs: compression header off-by-one, test_editor window/controller handling; fix or mark skipped with reason.

## Success Criteria
- CTest/CI default runs execute only stable + smokes and one rendering suite.
- Optional suites runnable via label/preset; fail early if pre-reqs missing.
- Documentation matches actual behavior.
- No regressions in core stable tests.

## Coordination
- Post progress/hand-offs to coordination-board.md.
- Use designated agent IDs above when claiming work.
