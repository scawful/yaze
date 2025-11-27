# HMagic regression checklist (for yaze tests)

Goal: ensure yaze parsers/editors don’t reproduce known hmagic bugs.

## Scenarios to test
1) **Overworld exits/items/whirlpools pointer integrity**
   - Edit exits/whirlpools/items, save, reload ROM; verify ending white dots (credits) and Ganon→Triforce entrance remain intact.
   - Validate pointer tables (no mass repoint to empty room).

2) **“Remove all overworld exits” safety**
   - Invoke bulk-delete; ensure credits dots and special entrances remain.
   - Verify no unintended entrance insertion/crash when editing global grid.

3) **Dungeon room pointer overflow**
   - Add room data until near limit; ensure pointer calc stays valid and refuses to corrupt or overrun; emits error instead of silent corruption.

4) **BG1/BG2 sprite handling**
   - Move BG2 markers (red dots) and confirm BG flag persists across save/reload.

5) **Monologue storage**
   - Round-trip text: decode → modify → encode; ensure no bank overflow, region switch honored, abs terminator respected; dictionary bounds enforced.

6) **Sprite names**
   - Decode `sprname.dat` standard format (flag=0, LE size, 0x11c length-prefixed entries) and ensure names load; if alt format (flag!=0) appears, validate 256×9-byte table.

## Implementation plan
- Add gtest cases using a US ROM fixture (vanilla.sfc):
  - Text: round-trip known messages and synthetic long messages; check max length enforcement.
  - Sprite names: decode standard/alt format blobs; compare to reference array.
  - Pointer safety: build synthetic overworld/dungeon tables and assert parsers reject OOB offsets.
- Add CLI fixtures for bulk operations (remove exits/items) and assert postconditions via serializers.
