# z3ed CLI Guide

The `z3ed` command-line tool provides scriptable ROM editing, AI-assisted workflows, and resource inspection. It ships with all `*-ai` preset builds and runs on Windows, macOS, and Linux.

---

## Building

```bash
# Build with AI features
cmake --preset mac-ai
cmake --build --preset mac-ai --target z3ed

# Run the text UI
./build/bin/z3ed --tui
```

> **Binary path note**: On macOS/Windows multi-config builds, use
> `./build/bin/Debug/z3ed` or `./build/bin/Release/z3ed`. Linux uses
> `./build/bin/z3ed`.

## AI Provider Configuration

AI features require at least one provider:

| Provider | Setup |
|----------|-------|
| **Ollama** (local) | `brew install ollama && ollama serve` |
| **Gemini** (cloud) | `export GEMINI_API_KEY=your_key` |
| **OpenAI** (cloud) | `export OPENAI_API_KEY=your_key` |
| **Anthropic** (cloud) | `export ANTHROPIC_API_KEY=your_key` |

Use `--ai_provider` to force a specific backend (e.g., `--ai_provider=openai`).
The default `--ai_provider=auto` selects the first configured provider.
Set the model with `--ai_model` (or `OLLAMA_MODEL` for Ollama).

> Without a provider, z3ed still works but agent commands use manual plans.

---

## Common Commands

| Task | Command |
|------|---------|
| ROM info | `z3ed rom-info --rom=zelda3.sfc` |
| List dungeon sprites | `z3ed dungeon-list-sprites --room=1 --rom=zelda3.sfc` |
| Describe dungeon room | `z3ed dungeon-describe-room --room=1 --rom=zelda3.sfc` |
| Describe overworld map | `z3ed overworld-describe-map --map=80 --rom=zelda3.sfc` |
| List messages | `z3ed message-list --rom=zelda3.sfc` |
| Search messages | `z3ed message-search --query="sword" --rom=zelda3.sfc` |

Commands follow `<noun>-<verb>` convention (dashes, not spaces). Use `--help` for flag details:
```bash
z3ed --help
z3ed help dungeon
z3ed help dungeon-list-sprites
```

---

## Mesen2 Live Debugging

These commands talk to a running **Mesen2-OoS** instance over its local socket
(`/tmp/mesen2-<pid>.sock`). No ROM is required for these commands.

| Task | Command |
|------|---------|
| Game state | `z3ed mesen-gamestate` |
| Active sprites | `z3ed mesen-sprites` |
| Active + inactive sprites | `z3ed mesen-sprites --all` |
| CPU registers | `z3ed mesen-cpu` |
| Read memory | `z3ed mesen-memory-read --address=0x7E0020 --length=16` |
| Write memory | `z3ed mesen-memory-write --address=0x7E0010 --data=01` |
| Disassemble | `z3ed mesen-disasm --address=0x008000 --count=20` |
| Trace | `z3ed mesen-trace --count=50` |
| Breakpoints | `z3ed mesen-breakpoint --action=add --address=0x008000` |
| Control | `z3ed mesen-control --action=pause` |

---

## AI Agent Workflows

### Interactive Chat

```bash
z3ed agent simple-chat --rom=zelda3.sfc --ai_provider=auto
```

Chat sessions maintain conversation history and can invoke ROM commands automatically.

### Agent Subcommands

| Command | Description |
|---------|-------------|
| `agent simple-chat` | Interactive AI chat |
| `agent test-conversation` | Automated test conversation |
| `agent plan` | Generate execution plan |
| `agent run` | Execute plan in sandbox |
| `agent diff` | Review proposal diff |
| `agent accept` | Apply proposal changes |
| `agent commit` | Save ROM changes |
| `agent revert` | Reload ROM from disk |
| `agent learn` | Manage learned knowledge |
| `agent todo` | Task management |
| `agent list` | List proposals |
| `agent describe` | Describe a proposal |

### Plan and Apply

```bash
# Create a plan without applying
z3ed agent plan --rom=zelda3.sfc

# List pending plans
z3ed agent list

# Review diff
z3ed agent diff --rom=zelda3.sfc

# Apply after review
z3ed agent accept --proposal-id=<id> --rom=zelda3.sfc
```

Plans are stored in `~/.yaze/proposals` (Windows uses `%USERPROFILE%\\.yaze\\proposals`).

### Scripted Chat

```bash
# From stdin
echo "Describe room 1" | z3ed agent simple-chat --rom=zelda3.sfc --stdout
```

---

## Best Practices

| Tip | Description |
|-----|-------------|
| **Use sandbox mode** | `--sandbox` flag creates a copy for safe testing |
| **Log sessions** | `--log-file agent.log` captures transcripts |
| **Structured output** | `--format json` or `--format yaml` for scripting |
| **Run tests after patches** | `./build/bin/yaze_test --unit` |
| **TUI command palette** | Press `:` in TUI mode to search commands |

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `agent simple-chat` hangs | Verify `ollama serve` is running or `GEMINI_API_KEY`/`OPENAI_API_KEY`/`ANTHROPIC_API_KEY` is set |
| `mesen-*` commands fail to connect | Launch Mesen2-OoS and ensure the socket exists under `/tmp/mesen2-*.sock` |
| Missing `libgrpc` or `absl` | Rebuild with `*-ai` preset |
| ROM not found | Use absolute paths or set `YAZE_DEFAULT_ROM` |
| Command not found | Run `z3ed --help` to verify build is current |
| Empty proposal diffs | Include `--rom` with `--sandbox` or `--workspace` |

---

## Related Documentation

- [Testing Without ROMs](../developer/testing-without-roms.md) - CI fixtures
- [Debugging Guide](../developer/debugging-guide.md) - Logging and instrumentation
- [CLI Reference](../cli/README.md) - Complete command documentation
