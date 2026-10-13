# Message System Architecture

**Status**: Draft
**Last Updated**: 2026-07-28
**Related Code**: `src/app/editor/message/`,
`src/cli/handlers/game/message_commands.cc`

This document outlines the architecture of the Message (Text) System in YAZE.

## Overview

The Message System manages the in-game dialogue and narration. ALttP uses a custom text engine with:
*   **Proportional Font**: Variable width characters.
*   **Dictionary Compression**: Common words/phrases are stored in a dictionary and referenced by single bytes to save space.
*   **Command Codes**: Byte sequences control window layout, scrolling, text speed, and player interaction.

## Data Structures

### 1. MessageData
Represents a single dialogue entry.
*   **ID**: Message index (0-396 in vanilla).
*   **Address**: ROM offset.
*   **RawString**: Human-readable text with dictionary tokens (e.g., `[D:01]`).
*   **ContentsParsed**: Fully expanded text (e.g., `Link`).
*   **Data**: Raw ROM bytes.

### 2. DictionaryEntry
A phrase used for compression.
*   **ID**: Index (0x00 - 0x60).
*   **Contents**: The text (e.g., " the ").
*   **Token**: Representation in `RawString` (e.g., `[D:00]`).

### 3. TextElement
Represents special control codes or characters.
*   **Commands**: `[W:02]` (Window Border), `[SPD:01]` (Scroll Speed).
*   **Special Chars**: `[UP]` (Arrow), `[A]` (Button), `...` (Ellipsis).

## ROM Layout (Vanilla)

*   **Primary block**: PC `0x0E0000-0x0E7FFF`, mapped to SNES
    `$1C:8000-$1C:FFFF` (32KB).
*   **Secondary block**: PC `0x075F40-0x0773FF`, mapped to SNES
    `$0E:DF40-$0E:F3FF` (5.3KB).
*   **Dictionary**: Pointers at `0x74703`.
*   **Font Graphics**: 2BPP tiles at `0x70000`.
*   **Character Widths**: Table at `0x74ADF`.

## Pipeline

### Loading
1.  **Read**: `ReadAllTextData` scans the ROM text blocks.
2.  **Parse**: Bytes are mapped to characters using `CharEncoder`.
3.  **Expand**: Dictionary references (`0x88`+) are looked up and replaced with `[D:XX]` tokens.
4.  **Preview**: `MessagePreview` renders the text to a bitmap using the font graphics and width table.

### Saving
1.  **Parse**: User text is converted to bytes.
2.  **Optimize**: `OptimizeMessageForDictionary` scans the text for dictionary phrases and replaces them with single-byte references.
3.  **Plan**: `BuildVanillaMessageSavePlan` serializes both physical blocks,
    requires the manifest's expected message count, and distinguishes the one
    standalone `[BANK]` command from command arguments such as `[W:80]`.
4.  **Preflight**: Project-backed callers analyze the plan's exact half-open PC
    write ranges against Hack Manifest ownership and write policy before any
    ROM mutation.
5.  **Write**: `ApplyVanillaMessageSavePlan` applies the immutable plan through
    an exact write fence. CLI persistence requires a backup, atomic ROM
    replacement, and independent reopen/readback before reporting success.

ASM-owned expanded banks use a separate source pipeline. A complete canonical
`yaze-message-bundle` is merged by bank-local ID, validated against the Hack
Manifest's exact expanded range/capacity, and rendered to a deterministic
no-`org` Asar include. `message-source-sync` is dry-run-first and publishes the
bundle/include pair only with an exact source SHA-256 CAS and rollback-backed
reopen verification. Write mode holds both an in-process mutex and the
persistent `.yaze-message-source-sync.lock` in each distinct publication-target
directory from canonical read through publication verification or rollback.
This keeps nested project descriptors in the same lock domain when they share
either artifact. It never writes the ROM.

## Editor UI

*   **Message List**: Filterable table (text, ID, or address) with a vanilla/expanded bank filter. The current row stays selected, and Alt+Up/Down moves through the visible rows.
*   **Editor**: Header with ID, bank, address, and size. Multiline text with a drag split above the live preview. Parse and line-width diagnostics sit between them.
*   **Find & Replace**: Persistent bar opened from the editor or Edit → Find. Find Next/Previous walk the catalog, Replace All still validates the whole batch before writing, and Find All filters the list.
*   **Reference**: The loaded text stays beside the draft. Accept makes the draft the new reference for that message.
*   **Tools**: Commands, symbols, and dictionary entries are tabs. Clicking a row inserts the token at the caret. Optimize compresses the current message with dictionary tokens.
*   **Font Atlas**: Dialogue font plus expanded-bank load, capacity, and add-message controls.
*   **Status bar**: Current message, byte size, line count, filter count, and unsaved/parse-error mode.

## Limitations

*   **Hardcoded Limits**: The text block sizes are fixed for vanilla.
*   **Translation**: The editor compares against the text loaded in this
    session. It does not load a second ROM or language pack.
*   **GUI source save**: Expanded dialogue can be previewed and published to
    `messages.source` from the font panel. Preview is bound to the canonical
    bundle SHA captured when that bundle was loaded and to the exact draft
    that was previewed. A disk change before preview, or a draft or disk
    change after preview, rejects publication. A successful publish clears
    only the expanded source-draft flag. Vanilla, font, and other ROM dirty
    state stay in place, and `write_policy=block` still refuses expanded ROM
    writes. The refusal points at the canonical bundle and generated include.
*   **Filesystem metadata**: Source publication and rollback preserve exact file
    contents and existing POSIX mode bits, not ownership, ACLs, extended
    attributes/flags, or target-specific Windows attributes/DACLs. Source
    artifacts with special filesystem metadata are not supported yet.
