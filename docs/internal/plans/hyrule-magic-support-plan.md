# Hyrule Magic and Parallel Worlds compatibility research

Status: ARCHIVE (historical proposal; unverified assumptions)

Owner (Agent ID): zelda3-hacking-expert

Last Reviewed: 2026-09-22

Canonical follow-up: `CO-1` in the
[editor capability parity plan](editor-capability-parity-plan.md).

## Purpose and status

The original proposal explored doctoring legacy Hyrule Magic ROMs and loading
Parallel Worlds layouts. It did not establish a complete compatibility
contract. It is preserved here as research context, not approved implementation
instructions or evidence that those ROMs are supported.

Feature parity with Hyrule Magic and ZScream is a separate question from ROM
layout compatibility. A familiar ROM title, extension, file size, or panel
name does not prove a safe data layout or authorize automatic repair.

## Historical hypotheses to revalidate

| Earlier idea | Evidence still required before implementation |
| --- | --- |
| Identify Hyrule Magic through header signatures | Versioned reference fixtures and a discriminating structural signature; the proposal did not establish one |
| Detect an erased bank 00; the earlier note cited `GoT-v040.smc` | Reproduce against a recorded digest and current `rom-doctor`; the old assertion "confirmed working" is historical and is not renewed here |
| Identify Parallel Worlds by internal title or byte sequences | Corroborated layout/version evidence; title matching alone must not select a writer |
| Use a dedicated Parallel Worlds dungeon loader | Locate and validate the actual pointer tables, bank/address mapping, and record limits against a named fixture |
| Vanilla room pointer address `0x21633` (PC) | Confirm header handling and source/version meaning before using it; no Parallel Worlds replacement offset was verified |
| Resize 1.5 MiB images or restore bank 00 from vanilla data | Establish which bytes are intentionally changed and define explicit repair scope; never infer safe repair from size or heuristic warnings |
| Automatically recalculate checksums | Prove a named repair is needed, preview affected bytes, and preserve originals; checksum success does not establish structural correctness |

The earlier `hm_support.cc`, `ParallelWorldsDungeonLoader`, and
`Rom::IsParallelWorlds()` names were proposed APIs. They are not requirements
or proof of current implementation. The old absolute/file-URI source paths
were machine-specific guesses and must not be reused.

## Execution contract

1. Start with `CO-1` and inspect current ROM loading, doctoring, and layout
   abstractions before proposing a new API.
2. Record the exact reference-editor version and legally supplied ROM digest.
   Verify read-only detection and parsing first, including rejection cases.
3. Define named read/write support independently. Work only on disposable
   copies and preserve original hashes through failed or unsupported operations.
4. Add a writer only after pointer, capacity, relocation, no-op, and
   save/reopen contracts are proven. Never route heuristic warnings to repair.
5. Update the canonical capability plan and public support boundaries with
   concrete evidence. Unknown offsets and untested layouts remain unsupported.

## Validation

This September 22 update classifies the original proposal and removes unsafe
implementation assumptions. It runs no ROM repair, write, load qualification,
or reference-editor compatibility test.
