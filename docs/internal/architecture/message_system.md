# Message System Architecture

**Status**: Draft
**Last Updated**: 2025-11-21
**Related Code**: `src/app/editor/message/`, `src/cli/handlers/game/message.cc`

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

*   **Bank 0E (0xE0000)**: Primary text data block (32KB).
*   **Bank 0E (0x75F40)**: Secondary text data block (5.3KB).
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
3.  **Write**: Data is written sequentially to the ROM text blocks. If the first block overflows, it spills into the second block.

## Editor UI

*   **Message List**: Displays all messages with ID and preview.
*   **Editor**: Multiline text input. Buttons to insert commands/special chars.
*   **Preview**: Live rendering of the message box as it would appear in-game.
*   **Dictionary**: Read-only view of dictionary entries.

## Limitations

*   **Hardcoded Limits**: The text block sizes are fixed for vanilla.
*   **Translation**: No specific tooling for side-by-side translation.
*   **Export**: Limited to binary "Expanded Messages" format; no JSON/YAML support.
