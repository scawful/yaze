# Getting Started with YAZE

YAZE is a ROM editor for "The Legend of Zelda: A Link to the Past" (US and JP versions). It provides a full-featured GUI editor, integrated SNES emulator, AI-powered command-line tools, and a web preview build.

---

## Quick Start

1. **Choose a build.** [v0.8.0-exp.1](https://github.com/scawful/yaze/releases/tag/v0.8.0-exp.1)
   is the published experimental prerelease. The stable release is v0.7.2;
   nightly/source builds can contain later features. Check the
   [version-specific release notes](../release-notes.md#v080).
2. **Copy your ROM into a separate folder**, outside a `.yazeproj` bundle.
3. **Launch** and open that ROM file directly via `File > Open ROM / Project`. Start with
   one small Dungeon room edit so the first save is easy to check.
4. **Save** with `File > Save ROM`, keeping backups enabled. If saving stops,
   resolve the named pending edit or validation error before continuing.
5. **Close and reopen the copied ROM.** Confirm that the room edit persisted
   before starting a larger editing session.

### Experimental Lyra projects

Copy the complete `.yazeproj` bundle with its assets. Before opening the copy,
check the paths in `project.yaze`: its ROM and writable assets must point into
the copy. Legacy projects can keep external paths after a folder copy.
Use an editor version that understands the project's document schema.
The published prerelease has the schema-3, 32-page Lyra document model; later
gameplay-area, transport and reward models belong to newer source candidates.
The [source and qualification matrix](../../internal/status.md#lyra-source-and-qualification-boundaries)
distinguishes them. First verify normal project Save and fresh reopen. A saved
project alone does not establish a playable Oracle Build/Test result.

> **Using .yazeproj bundles?** See the [.yazeproj Bundle Guide](../usage/yazeproj-bundles.md) for how to open bundles on each platform.

> **Building from source?** See the [Build and Test Quick Reference](../build/quick-reference.md).

> **Helping beta-test?** See the [Beta Testing Guide](../usage/beta-testing.md)
> for what to try first, ROM compatibility expectations, and useful bug report
> details.

---

## Web App (Preview)

Want a quick browser-based preview? Use the web build with a limited feature
set and no emulator support. See the [Web App Guide](../usage/web-app.md) for
supported features and AI configuration.

---

## Tips

- **Backups**: Automatic backups are enabled by default. Each save creates a
  timestamped backup. Use `File > ROM Backups... > Restore` to stage a backup
  in the current session, inspect it, then use `Save ROM` to commit it. Keep
  **Backup Before Save** enabled so that commit also preserves the ROM version
  it replaces. Autosave remains paused until you either commit with `Save ROM`
  or choose `Discard Restored Backup` in the ROM Backups popup to abandon the
  staged ROM-buffer edits and reload the unchanged backing ROM. Resolve pending
  dungeon-room or palette edits before discarding.
- **Overworld vs. World Map**: the Overworld Editor edits playable areas; Screen
  Editor > Overworld Map edits the pause-menu map art.
- **Experiment Flags**: Try new features via `File > Options > Experiment Flags`.
- **Editor limits**: Graphics has experimental guarded 3bpp sheet write-back;
  unsupported graphics and Screen edits still block ROM save. Read the
  version-specific release notes and Beta Testing Guide before testing persistence.
- **Extensions**: Load custom tools from the `Extensions` menu (plugin system under development).
- **AI Providers**: Configure providers in `Settings > Agent` or set
  `GEMINI_API_KEY`, `OPENAI_API_KEY`, or `ANTHROPIC_API_KEY`.

---

## Editor Status

| Editor | Tester status | Notes |
|--------|---------------|-------|
| Dungeon | Tester ready | Use a copied ROM; save, close, and reopen after a small room edit. |
| Overworld | Tester ready | Use a copied ROM; save, close, and reopen after a small map edit. |
| Message | Tester ready | Save valid parsed text and verify it after reopening. |
| Palette | Conditional | Palette **Save to ROM**, then File > Save ROM. Both steps are required. |
| Assembly / Asar | Conditional | Source save and patch application are separate advanced workflows. |
| Sprite | Conditional | Custom `.zsm` editing; vanilla room sprites are edited in Dungeon. |
| Settings | Conditional | Verify non-ROM settings after restarting Yaze. |
| Graphics | Experimental | Guarded 3bpp sheet write-back; verify Save/reopen on a copy. Unsupported edits remain blocked. |
| Screen | View only | Pending edits deliberately block coordinated ROM save. |
| Hex / Memory | View only | Advanced raw tooling; no complete dirty/undo/save contract. |
| Music | View only | Playback and inspection; ROM persistence is incomplete. |
| Emulator | Experimental | Runtime emulator; save-state UI incomplete. |
| Agent | Experimental | Availability depends on build and provider configuration. |

The [editor readiness matrix](../reference/feature-coverage-report.md) is the
canonical detailed status.

---

## Command-Line Interface (z3ed)

The `z3ed` CLI provides scriptable access to ROM editing capabilities.

### AI Chat

```bash
z3ed agent simple-chat --rom=zelda3.sfc --ai_provider=auto
```
Example prompt: "What sprites are in room 1?"

### ROM Inspection

```bash
# List sprites in a dungeon room (room 1 = Eastern Palace)
z3ed dungeon-list-sprites --room=1 --rom=zelda3.sfc

# Describe overworld map
z3ed overworld-describe-map --map=80 --rom=zelda3.sfc

# Search messages
z3ed message-search --query="Master Sword" --rom=zelda3.sfc
```

For more details, see the [z3ed CLI Guide](../usage/z3ed-cli.md).

---

## Next Steps

- **[Dungeon Editor Guide](../usage/dungeon-editor.md)** - Learn dungeon room editing
- **[z3ed CLI Guide](../usage/z3ed-cli.md)** - Master the command-line interface
- **[Architecture Overview](../developer/architecture.md)** - Understand the codebase
