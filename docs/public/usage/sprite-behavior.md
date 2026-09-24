# Sprite behavior actions

The **Behavior** tab authors a small action model alongside a ZSM visual asset.
The current export profile is `oracle_actions_v1`. It uses reviewed Oracle macros
and helpers. It does not import or replace a sprite's existing hand-written behavior.
Vanilla preview copies can use this model when targeting an Oracle project; there
is no native vanilla behavior export profile yet.

## Author and review a model

1. Open or import an asset in **Custom Sprites** and configure the project's
   `sprite_source_root`. In **Behavior**, choose **Use reviewed Oracle action
   profile**. Yaze verifies the expected macro, helper, and symbol contracts and
   records their source hashes.
2. Add up to 16 actions and assign each an animation from the **Animations** tab.
   **Preview action animation** previews only the assigned visual sequence.
   Movement, dialogue, and collision are not simulated in the canvas.
3. Configure player blocking, solicited dialogue, XY movement/tile bounce, or a
   timer transition. Transition targets refer to other actions, not sprite
   subtypes. Incoming transitions must be redirected before deleting an action.
   Referenced animations cannot be deleted until actions use another animation.
4. Enter a unique **ASM label prefix**, then **Build behavior candidate** to inspect
   the generated text. **Copy behavior candidate** rechecks source hashes and
   generates fresh text. Neither action writes ASM files, registers a sprite,
   changes placement, or patches a ROM.
5. Save the ZSM, then save the project. Behavior models live in version-2 project
   asset records. Assets without behavior continue using version-1 records. ZSM
   files retain their original format and do not carry the action model alone.

## Action semantics

| Setting | Effect |
|---|---|
| Animation | References a ZSM animation group; frame ranges and wait remain authoritative there |
| Block player | Calls Oracle's barrier helper; contact can also cancel Link's dash/hookshot |
| Solicited dialogue | Uses an existing message ID when interaction succeeds; it does not allocate or author messages |
| On message accepted | Transitions when the message opens, not when it closes or a response is selected; **Stay (no re-entry)** returns without resetting action state |
| XY velocity | Set once on action entry, in sixteenths of a pixel per game tick; movement is applied once per Main update |
| Tile bounce | Checks collision and reverses the relevant velocity before movement; requires XY movement |
| Timer transition | Sets Timer A on entry and transitions when it reaches zero; zero means the next Main update can transition immediately |

Each generated entry sets `SprAction`, `SprFrame`, `SprTimerB`, `SprTimerA`,
`SprXSpeed`, and `SprYSpeed`. Animation Timer B starts with the animation wait so
the initial frame receives a full interval. A self-transition re-enters and resets
these fields. Staying after a message does not. Movement velocities are not reset
each update, so tile bounce can retain its new direction.

The per-update order is animation, player blocking, dialogue transition, timer
transition, tile bounce, then movement. Dialogue takes precedence if both transition
conditions occur together. Taking a transition returns through the target entry;
it does not run the target's update code in that same call.

Oracle's animation macro computes `end + 1` in an 8-bit immediate, so this profile
requires endpoints below 255 and a nonzero wait. Speed -128 cannot be negated in an
8-bit velocity; bounce rejects it. The UI uses speeds -127 through 127.

## Integration contract

Generated `<prefix>_Init` and `<prefix>_Main` are same-bank `JSR` routines returning
with `RTS`. The caller must supply 8-bit A/X/Y, X as the current sprite slot, and a
data bank that maps the low WRAM addresses used by the helpers. Keep the routines
and dispatch table in the same bank. Set the draw frame-base term to zero for these
animation indices, or independently prove a compatible draw adapter.

Call Init once after the engine's property reset/Prep setup, where it cannot be
wiped by a later reset. Call Main through the family's active/freeze gate. The
engine, not this model, decrements timers. Its freeze/menu/transition scheduling,
dynamic-spawn initialization, and shared-family RAM ownership require review.
The candidate does not establish those conditions or install any call site.

Do not call Init unconditionally for every variant sharing an ID. Existing family
registration, subtype dispatch, Prep logic, and property defaults remain owned by
the game source. This model does not provide selector transport or new RAM.

A source change blocks export. **Recheck and bind reviewed sources** can accept
non-code changes only while the normalized code still matches the reviewed profile.
Changed instructions or symbols require an adapter review. The source hashes do
not attest the ROM's actual bytes, other patches, or a successful game build.

Current omissions include dialogue-choice graphs, unconditional cutscene messages,
contact-damage policy, targeting/detection, spawn ownership, search/return routing,
script importing, and native vanilla runtime generation. Keep those in existing
hand-written routines until corresponding contracts are implemented.

See [visual authoring](sprite-authoring.md),
[agent contracts](../../internal/architecture/sprite-behavior.md), and
[the sprite standard](../../internal/architecture/sprite-catalog.md).
