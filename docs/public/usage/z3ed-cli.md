# z3ed CLI Guide

The `z3ed` command-line tool provides scriptable ROM editing, AI-assisted workflows, and resource inspection. It ships with all `*-ai` preset builds and runs on Windows, macOS, and Linux.

---

## Building

```bash
# Build with AI features
cmake --preset mac-ai
cmake --build --preset mac-ai --target z3ed

# Check CLI help (wrapper auto-selects a suitable z3ed binary)
./scripts/z3ed --help
```

> **Binary path note**: On macOS/Windows multi-config builds, use
> `./build/bin/Debug/z3ed` or `./build/bin/Release/z3ed`. Linux uses
> `./build/bin/z3ed`. `./scripts/z3ed` will auto-select from common build dirs.

## AI Provider Configuration

AI features require at least one provider:

| Provider | Setup |
|----------|-------|
| **Ollama** (local) | `brew install ollama && ollama serve` (optional: `export OLLAMA_HOST=http://localhost:11434`) |
| **Gemini** (cloud) | `export GEMINI_API_KEY=your_key` |
| **OpenAI** (cloud) | `export OPENAI_API_KEY=your_key` |
| **LM Studio** (local, OpenAI-compatible) | Start LM Studio server and pass `--ai_provider=openai --openai_base_url=http://localhost:1234` (or set `OPENAI_BASE_URL`) |
| **Anthropic** (cloud) | `export ANTHROPIC_API_KEY=your_key` |

Use `--ai_provider` to force a specific backend (e.g., `--ai_provider=openai`).
The default `--ai_provider=auto` selects the first configured provider.
Set the model with `--ai_model` (or `OLLAMA_MODEL` for Ollama). You can also
override local endpoints with `OLLAMA_HOST` or `OPENAI_BASE_URL` (alias:
`OPENAI_API_BASE`). For OpenAI-compatible local servers (LM Studio), set
`--openai_base_url` (or `OPENAI_BASE_URL`) and provide `--ai_model` to match
your local model name.

> Without a provider, z3ed still works but agent commands use manual plans.

---

## Common Commands

| Task | Command |
|------|---------|
| ROM info | `z3ed rom-info --rom=zelda3.sfc` |
| Read ROM bytes | `z3ed rom read --address=0x1000 --length=16 --rom=zelda3.sfc` |
| List dungeon sprites | `z3ed dungeon-list-sprites --room=1 --rom=zelda3.sfc` |
| Describe dungeon room | `z3ed dungeon-describe-room --room=1 --rom=zelda3.sfc` |
| Show minecart collision (Oracle) | `z3ed dungeon-list-custom-collision --room=0x77 --tiles=0xB7,0xB8,0xB9,0xBA --rom=roms/oos168.sfc` |
| Minecart audit (Oracle) | `z3ed dungeon-minecart-audit --rooms=0x77,0xA8,0xB8 --only-issues --rom=roms/oos168.sfc` |
| ASCII room map (Oracle overlay) | `z3ed dungeon-map --room=0x77 --rom=roms/oos168.sfc` |
| Describe overworld map | `z3ed overworld-describe-map --map=80 --rom=zelda3.sfc` |
| List messages | `z3ed message-list --rom=zelda3.sfc` |
| Search messages | `z3ed message-search --query="sword" --rom=zelda3.sfc` |

Most commands follow the `<noun>-<verb>` convention (dashes, not spaces), and
some use subcommands like `rom read` or `debug state`. Use `--help` for flag details:
```bash
z3ed --help
z3ed help dungeon
z3ed help dungeon-list-sprites
```

---

## Mesen2 Live Debugging

These commands talk to a running **Mesen2-OoS** instance over its local socket
(`/tmp/mesen2-<pid>.sock`). No ROM is required for these commands. The `debug`
alias is the recommended front door (Mesen2 is the default backend).

| Task | Command |
|------|---------|
| Game state | `z3ed debug state` |
| Active sprites | `z3ed debug sprites` |
| Active + inactive sprites | `z3ed debug sprites --all` |
| CPU registers | `z3ed debug cpu` |
| Read memory | `z3ed debug mem read --address=0x7E0020 --length=16` |
| Write memory | `z3ed debug mem write --address=0x7E0010 --data=01` |
| Disassemble | `z3ed debug disasm --address=0x008000 --count=20` |
| Trace | `z3ed debug trace --count=50` |
| Breakpoints | `z3ed debug breakpoint --action=add --address=0x008000` |
| Control | `z3ed debug control --action=pause` |

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
| **Structured output** | `--format json` or `--format text` for scripting |
| **Run tests after patches** | `./build/bin/yaze_test --unit` |
| **TUI command palette** | Press `:` in TUI mode to search commands |

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `agent simple-chat` hangs | Verify `ollama serve` is running or `GEMINI_API_KEY`/`OPENAI_API_KEY`/`ANTHROPIC_API_KEY` is set |
| `mesen-*` commands fail to connect | Launch Mesen2-OoS and ensure the socket exists under `/tmp/mesen2-*.sock` (or set `--mesen-socket`) |
| Missing `libgrpc` or `absl` | Rebuild with `*-ai` preset |
| ROM not found | Use absolute paths or set `YAZE_DEFAULT_ROM` |
| Command not found | Run `z3ed --help` to verify build is current |
| Empty proposal diffs | Include `--rom` with `--sandbox` or `--workspace` |

---

## Related Documentation

- [Testing Without ROMs](../developer/testing-without-roms.md) - CI fixtures
- [Debugging Guide](../developer/debugging-guide.md) - Logging and instrumentation
- [CLI Reference](../cli/README.md) - Complete command documentation
- [API Reference](../reference/api.md) - HTTP + gRPC automation endpoints
