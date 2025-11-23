# Message System Improvement Plan

**Status**: Proposal
**Last Updated**: 2025-11-21

This document outlines a plan to enhance the dialogue editing capabilities of YAZE, focusing on translation workflows and data portability.

## 1. JSON Import/Export

**Goal**: Enable external editing and version control of text.

*   **Format**:
    ```json
    [
      {
        "id": 0,
        "address": 917504,
        "text": "[W:00][SPD:00]Welcome to [D:05]...",
        "context": "Uncle dying in sewers"
      }
    ]
    ```
*   **Implementation**:
    *   Add `SerializeMessages()` and `DeserializeMessages()` to `MessageData`.
    *   Integrate with the existing CLI `export` commands.

## 2. Translation Workspace

**Goal**: Facilitate translating the game into new languages.

*   **Side-by-Side View**: Show the original text (Reference) next to the editable text (Translation).
*   **Reference Source**: Allow loading a second "Reference ROM" or a JSON file to serve as the source text.
*   **Dictionary Management**:
    *   **Auto-Optimization**: Analyze the full translated text to propose a *new* optimal dictionary for that language.
    *   **Manual Editing**: Allow users to define custom dictionary entries.

## 3. Expanded Text Support

**Goal**: Break free from vanilla size limits.

*   **Repointing**: Allow the text blocks to be moved to expanded ROM space (Banks 10+).
*   **Bank Management**: Handle bank switching commands automatically when text exceeds 64KB.

## 4. Search & Replace

**Goal**: Global editing operations.

*   **Regex Support**: Advanced search across all messages.
*   **Batch Replace**: "Replace 'Hyrule' with 'Lorule' in all messages".

## 5. Scripting Integration

**Goal**: Allow procedural generation of text.

*   **Lua/Python API**: Expose message data to the scripting engine.
*   **Usage**: "Generate 100 variations of the shopkeeper dialogue".
