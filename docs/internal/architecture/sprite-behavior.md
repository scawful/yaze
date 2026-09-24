# Sprite action profile contract

Status: authoring and candidate generation; no runtime installation. Read this
alongside [the sprite standard](sprite-catalog.md) before extending behavior code.

## Model and source authority

`core/sprite_behavior.h` defines ordered actions. An action index is `SprAction`,
not main ID, subtype, appearance, or encounter parameter. The initial backend is
`oracle_actions_v1`; unknown profiles must fail. No inference from existing ASM is
performed. A newly enabled model is a draft, not an imported character behavior.

`SpriteAssetBinding::behavior` persists in asset record version 2. Version 1 remains
readable and is still emitted for assets without behavior. Version 2 adds exactly
one `behavior` object with `profile`, `source_sha256` (three entries), and `actions`.
Each action contains `name`, `animation`, `block_player`, `message_id`,
`message_next`, `move`, `x_speed`, `y_speed`, `bounce_tiles`, `timer_ticks`, and
`timer_next`. Unknown fields, malformed types, out-of-range values, unsupported
profiles, and invalid action targets are rejected. Limit actions to 1–16.

`animation` is an index into the ZSM animation list. No duplicate copy of its
range/wait is stored in the behavior. Deleting a referenced animation is blocked;
removing another animation remaps higher indices. Export revalidates ranges against
the currently opened asset because an external ZSM edit may invalidate references.

The source root supplies three fixed dependencies, using the catalog's bounded,
root-contained regular-file reader:

| File | Reviewed normalized-code SHA-256 |
|---|---|
| `Core/sprite_macros.asm` | `07a4aca7fab35159c10abe0804f5de6f2455c32ed1acc7f00abe69b4ad09acd8` |
| `Core/sprite_functions.asm` | `d727ced1802805fbebada4d5bcffca4f366adcb23c14ef9bb1985afa48b93f39` |
| `Core/symbols.asm` | `5914fb1de28a4612cfac189d20d22586a04526f5f6d0d3a2307489413dee22ab` |

Normalization removes semicolon comments and whitespace, preserving code case.
These fingerprints deliberately cover the complete files; unrelated code changes
also require review in this first backend. Do not automatically update fingerprints
when a test fails. Exact whole-file hashes are separately pinned to the asset when
the profile is bound. Export checks both reviewed code and these exact snapshots,
plus the visual asset's draw-source fence when one exists.

## Verified primitive contracts

Current Oracle source and USDASM labels were inspected for these contracts. They
are source evidence, not proof that a user's loaded ROM contains these routines.

| Primitive | Inputs and effects | Control-flow contract |
|---|---|---|
| `%PlayAnimation(start,end,wait)` | X slot; reads/writes `SprFrame` and `SprTimerB`; A/flags clobbered | No movement; timer decrement is external; reject end=255 and wait=0 |
| `%ShowSolicitedMessage(id)` | Loads A=message low and Y=high; calls `$05E1A7`; writes message globals `$1CF0/$1CF1` and on success the sprite cooldown `$0F10,X`; A/Y/flags and helper scratch are not preserved | Carry indicates message accepted/opened; consume it immediately, not after another operation; this is not dialogue completion |
| `Sprite_PlayerCantPassThrough` | Alias `$1EF4F3`; temporarily clears/restores hitbox flags and checks same-layer player contact; collision can cancel hookshot/dash and halt player movement | JSL/RTL; no generic collision-result branch is inferred |
| `Sprite_BounceFromTileCollision` | Calls collision long entry `$06E496`; X collision bits `$03` negate X speed, Y bits `$0C` negate Y speed; updates collision/helper scratch | JSL/RTL; it does not move; reject -128 velocities when bounce is enabled |
| `Sprite_Move` | Oracle long helper applies fixed-point XY speeds to low/high coordinates and fractional position accumulators; A/Y/flags clobbered | JSL/RTL, exactly one call per moving update; X remains the actor slot |
| `JumpTableLocal` | A action index; inline same-bank word table; uses direct-page `$00–$03` and consumes its long-call return to dispatch | Called with JSL; target actions return RTS to the original same-bank caller; invalid action index returns before dispatch |

Evidence locations: Oracle `Core/sprite_macros.asm` (PlayAnimation,
ShowSolicitedMessage, MoveTowardPlayer, GotoAction), `Core/sprite_functions.asm`
(Sprite_MoveHoriz/Vert/Move, Sprite_BounceFromTileCollision), and `Core/symbols.asm`.
USDASM entry points: `$008781`, `$05E1A7`, `$06E496`, `$1EF4F3`.

Do not repeat two stale skill assumptions: `%GotoAction` writes only `SprAction`;
`%MoveTowardPlayer` sets speed **and** calls `Sprite_MoveLong`. The current backend
uses neither macro. It emits explicit entry writes and a single movement call.

## Generated ownership and ordering

Generate unique, validated ASCII label prefixes. User-visible action names are not
interpolated into ASM. Emit only local routines and tables—no `org`, registration,
hooks, includes, ID allocation, or source writes.

Each entry writes action index, the linked animation's first frame, Timer B=wait,
Timer A=action ticks, and XY velocities. Stationary entries write zero velocity.
These six fields are reserved by this profile; do not install it where an existing
family uses them for another purpose. Dialogue and collision helpers have the
additional effects listed above. No new RAM is allocated.

Update order: animation → player blocking → solicited message → Timer A transition
→ tile bounce → XY movement → RTS. Message success either jumps to the target
entry or returns unchanged when target=-1. Message failure proceeds to the timer.
A taken transition returns after entry initialization, so there is no double update
or double movement in one call. All entry effects are explicit, including self-
transitions. Timer zero causes a transition on the next update, not recursive entry.

Caller requirements: A/X/Y eight-bit; X valid actor slot; DBR low-WRAM-visible;
same-bank JSR/RTS; local dispatch table and routines in the same bank. The caller
retains drawing, active/freeze checks, frame-base setup, main-ID/variant dispatch,
property reset and Prep ordering, dynamic spawn/respawn setup, and engine timer
scheduling. Initializing before a later property reset is invalid. The model does
not prove pause behavior by merely skipping Main.

Do not confuse the presence of this generator with shared-family integration.
`oracle.stalfos_patrol` remains unbound: this profile supplies no family, selector,
loader hook, overworld side table, contact policy, probe ownership, or search state.

## Implementation and verification

- Model/schema: `src/core/sprite_behavior{,_json}.h`, asset schema integration in
  `sprite_asset_json.h`.
- Reviewed source fence, validation, transitions, generation:
  `src/app/editor/sprite/sprite_behavior.h`.
- Public editor operations and Behavior tab:
  `src/app/editor/sprite/sprite_behavior_panel.cc`, `sprite_editor.{h,cc}`.
- Tests: `test/unit/editor/sprite_behavior_test.cc` plus existing sprite suites.
- [User guide](../../public/usage/sprite-behavior.md).

Source tests require `YAZE_ORACLE_SOURCE_ROOT`. The assembly test uses reviewed macro
bodies, explicit link symbols, and an in-memory synthetic ROM; it does not assemble
Oracle or execute the candidate. Keep pure validation, source checks, assembler
encoding, manual UI, candidate-ROM execution, and gameplay acceptance distinct.
