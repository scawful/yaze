# AI Model Ops Integration (YAZE)

- Status: ACTIVE
- Owner: ai-infra-architect
- Created: 2026-02-25
- Last Reviewed: 2026-02-25
- Next Review: 2026-03-04
- Canonical External Plan: Zelda Model Training Infrastructure (AFS + z3dk + distillation)

## Objective

Bridge external model-training outputs (AFS/z3dk/MLX) into reliable YAZE runtime usage across cloud providers and custom local models.

## Scope

In scope (YAZE repo):
- Provider/runtime integration and aliasing (`claude`, `chatgpt`, `lmstudio`)
- Rollout safety checks (multi-provider smoke)
- Operator docs for provider routing and release gates

Out of scope (external repos):
- Dataset generation and distillation logic in `~/src/lab/afs-scawful`
- z3dk reference JSON authoring in `~/src/hobby/z3dk`
- MLX LoRA training runs in `~/src/training`

## Current State

Implemented in this cycle:
- Provider aliases in `service_factory`:
  - `claude` => `anthropic`
  - `chatgpt` / `lmstudio` => `openai`
- Provider alias unit tests in `test/unit/cli/ai_service_factory_test.cc`
- Provider rollout smoke script:
  - `scripts/dev/ai-provider-matrix-smoke.sh`
  - Emits pass/fail/skip summary and optional JSON report
- Docs updated:
  - `docs/public/cli/README.md`
  - `docs/public/usage/z3ed-cli.md`
  - `docs/public/developer/ai-assisted-development.md`

## Integration Contract (External -> YAZE)

External model plan publishes model artifacts + IDs. YAZE expects:
1. Stable provider/model identifiers (`--ai_provider`, `--ai_model`)
2. OpenAI-compatible endpoint support for local/custom serving (`OPENAI_BASE_URL`)
3. Pre-rollout matrix evidence from `ai-provider-matrix-smoke.sh`

## Validation / Exit Criteria

Automated:
- `yaze_test_unit --gtest_filter='AIServiceFactoryTest.*'` passes
- `scripts/dev/ai-provider-matrix-smoke.sh --providers mock` passes

Operational:
- Provider matrix run with configured providers saved to JSON evidence
- No fallback-to-mock surprises when explicit provider is requested

## Next Steps

1. Add provider profile presets (task-specific routing: explain/code/vision) to avoid manual flag churn.
2. Add `z3ed agent provider-health --format=json` for CI-friendly readiness checks.
3. Wire matrix smoke into release readiness job as non-blocking artifact step.
