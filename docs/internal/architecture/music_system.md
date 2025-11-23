# Music System Architecture

**Status**: Draft
**Last Updated**: 2025-11-21
**Related Code**: `src/zelda3/music/`, `src/app/editor/music/`

This document outlines the architecture of the Music System in YAZE, covering both the editor and the underlying engine.

## Overview

The Music System is designed to edit the soundtrack of *A Link to the Past*, which runs on the SNES **N-SPC** audio engine. The system consists of:
1.  **Tracker Backend** (`src/zelda3/music/`): Parses binary ROM data into editable structures.
2.  **Music Editor** (`src/app/editor/music/`): Provides a UI for playback and modification.
3.  **Emulator Integration**: Uses the internal `Spc700` emulation for live preview.

## Core Components

### 1. The Tracker (`tracker.h`, `tracker.cc`)
Derived from the legacy "Hyrule Magic" C codebase, this class handles the low-level complexity of the N-SPC format.

*   **Data Structures**:
    *   `SpcCommand`: A doubly-linked list node representing a single music event (note, rest, command).
    *   `Song`: A collection of `SongPart`s (tracks), typically 8 channels.
    *   `SongRange`: Metadata mapping a ROM address range to parsed commands.
    *   `ZeldaInstrument`: ADSR and sample definitions.
*   **Parsing**:
    *   `LoadSongs`: Iterates through the game's pointer tables (Banks 0x1A, 0x1B) to load all music.
    *   `LoadSpcCommand`: Recursive descent parser for the byte-code stream.
*   **Serialization**:
    *   `SaveSongs`: Re-packs the linked lists into binary blocks.
    *   `AllocSpcBlock`: Manages memory for the binary output.

### 2. Music Editor (`music_editor.cc`)
The frontend GUI built with ImGui.

*   **Playback**:
    *   `PlaySong(int id)`: Writes to game RAM (`$7E012C`) to trigger the in-game song request mechanism via the emulator.
*   **Visualization**:
    *   `DrawPianoRoll`: Renders note data (currently a placeholder).
    *   `DrawToolset`: Transport controls (Play/Stop/Rewind).

### 3. SPC700 Audio Engine
The SNES audio subsystem (APU) runs independently of the main CPU.
*   **Communication**: The CPU uploads music data to the APU RAM (ARAM) via a handshake protocol on ports `$2140-$2143`.
*   **Banks**:
    *   **Overworld**: Bank `$1A`
    *   **Underworld**: Bank `$1B`
    *   **Credits**: Bank `$1A` (offset)

## Data Flow

1.  **Loading**: `MusicEditor::Initialize` -> `Tracker::LoadSongs` -> Parses ROM -> Populates `std::vector<Song>`.
2.  **Editing**: User modifies `SpcCommand` linked lists (Not yet fully implemented in UI).
3.  **Preview**: User clicks "Play". Editor writes ID to emulated RAM. Emulator NMI handler sees ID, uploads data to SPC700.
4.  **Saving**: `Tracker::SaveSongs` -> Serializes commands -> Writes to ROM buffer -> Fixes pointers.

## Limitations

*   **Vanilla-Centric**: The `Tracker` currently assumes vanilla bank sizes and offsets.
*   **Legacy Code**: The parsing logic is essentially a C port and uses raw pointers/malloc heavily.
*   **No Expansion**: Does not support the "Expanded Music" hack (relocated pointers) or "NewSPC" engine.
