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

## AI Provider Configuration

AI features require at least one provider:

| Provider | Setup |
|----------|-------|
| **Ollama** (local) | `brew install ollama && ollama serve` |
| **Gemini** (cloud) | `export GEMINI_API_KEY=your_key` |

Set the model with `--ai_model` or `OLLAMA_MODEL` environment variable.

> Without a provider, z3ed still works but agent commands use manual plans.

---

## Common Commands

| Task | Command |
|------|---------|
| Apply assembly patch | `z3ed asar patch.asm --rom zelda3.sfc` |
| List dungeon sprites | `z3ed dungeon list-sprites --dungeon 2 --rom zelda3.sfc` |
| Describe overworld map | `z3ed overworld describe-map --map 80 --rom zelda3.sfc` |
| Export palettes | `z3ed palette export --rom zelda3.sfc --output palettes.json` |
| Validate ROM | `z3ed rom info --rom zelda3.sfc` |

Commands follow `<noun> <verb>` convention. Use `--help` for flag details:
```bash
z3ed dungeon --help
z3ed dungeon list-sprites --help
```

---

## AI Agent Workflows

### Interactive Chat

```bash
z3ed agent chat --rom zelda3.sfc --theme overworld
```

Chat sessions maintain conversation history and can invoke ROM commands automatically.

### Plan and Apply

```bash
# Create a plan without applying
z3ed agent plan --prompt "Move eastern palace entrance 3 tiles east" --rom zelda3.sfc

# List pending plans
z3ed agent list

# Apply after review
z3ed agent accept --proposal-id <id> --rom zelda3.sfc
```

Plans are stored in `$XDG_DATA_HOME/yaze/proposals/` (or `%APPDATA%\yaze\proposals\` on Windows).

### Scripted Prompts

```bash
# From file
z3ed agent simple-chat --file queries.txt --rom zelda3.sfc --stdout

# From stdin
echo "Describe tile 0x3A in map 0x80" | z3ed agent simple-chat --rom zelda3.sfc --stdout
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
| `agent chat` hangs | Verify `ollama serve` is running or `GEMINI_API_KEY` is set |
| Missing `libgrpc` or `absl` | Rebuild with `*-ai` preset |
| ROM not found | Use absolute paths or set `YAZE_DEFAULT_ROM` |
| Command not found | Run `z3ed --help` to verify build is current |
| Empty proposal diffs | Include `--rom` with `--sandbox` or `--workspace` |

---

## Related Documentation

- [Testing Without ROMs](../developer/testing-without-roms.md) - CI fixtures
- [Debugging Guide](../developer/debugging-guide.md) - Logging and instrumentation
- [CLI Reference](../cli/README.md) - Complete command documentation
