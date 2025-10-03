# LLM Integration Architecture

**Visual Overview of z3ed Agent System with LLM Providers**

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         User / Developer                             │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                             │ Natural Language Prompt
                             │ "Make soldier armor red"
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       z3ed CLI (Entry Point)                         │
│                                                                       │
│  ┌────────────────────────────────────────────────────────────┐     │
│  │  z3ed agent run --prompt "..." --rom zelda3.sfc --sandbox  │     │
│  └────────────────────────────────────────────────────────────┘     │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                             │ Invoke
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Agent Command Handler                             │
│                  (src/cli/handlers/agent/)                           │
│                                                                       │
│  • Parse arguments                                                   │
│  • Create proposal                                                   │
│  • Select AI service ◄────────── Environment Variables              │
│  • Execute commands                                                  │
│  • Track in registry                                                 │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                             │ Get Commands
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      AI Service Factory                              │
│                   (CreateAIService() helper)                         │
│                                                                       │
│  Environment Detection:                                              │
│  • YAZE_AI_PROVIDER=ollama   → OllamaAIService                       │
│  • GEMINI_API_KEY set        → GeminiAIService                       │
│  • CLAUDE_API_KEY set        → ClaudeAIService                       │
│  • Default                   → MockAIService                         │
└─────────┬───────────────────┬────────────────┬───────────────────────┘
          │                   │                │
          │                   │                │
          ▼                   ▼                ▼
┌──────────────────┐ ┌──────────────┐ ┌─────────────────┐
│ OllamaAIService  │ │ GeminiAI     │ │ ClaudeAIService │
│                  │ │ Service      │ │                 │
│ • Local LLM     │ │ • Remote API │ │ • Remote API    │
│ • Free          │ │ • API Key    │ │ • API Key       │
│ • Private       │ │ • $0.10/1M   │ │ • Free tier     │
│ • Fast          │ │   tokens     │ │ • Best quality  │
└────────┬─────────┘ └──────┬───────┘ └────────┬────────┘
         │                  │                   │
         │                  │                   │
         ▼                  ▼                   ▼
┌──────────────────────────────────────────────────────────┐
│              AIService Interface                          │
│                                                            │
│  virtual absl::StatusOr<vector<string>>                   │
│      GetCommands(const string& prompt) = 0;               │
└────────────────────────────┬──────────────────────────────┘
                             │
                             │ Return Commands
                             ▼
                   ["rom validate --rom zelda3.sfc",
                    "palette export --group sprites ...",
                    "palette set-color --file ... --color FF0000"]
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Command Execution Engine                          │
│                                                                       │
│  For each command:                                                   │
│  1. Parse command string                                             │
│  2. Lookup handler in ModernCLI registry                            │
│  3. Execute in sandbox ROM                                           │
│  4. Log to ProposalRegistry                                          │
│  5. Capture output/errors                                            │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Proposal Registry                               │
│                 (Cross-session persistence)                          │
│                                                                       │
│  • Proposal metadata (ID, timestamp, prompt)                        │
│  • Execution logs (commands, status, duration)                      │
│  • ROM diff (before/after sandbox state)                            │
│  • Status (pending, accepted, rejected)                             │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Human Review (GUI)                                │
│                 YAZE Editor → Debug → Agent Proposals                │
│                                                                       │
│  • View proposal details                                             │
│  • Inspect ROM diff visually                                         │
│  • Test in GUI editors                                               │
│  • Accept → Merge to main ROM                                        │
│  • Reject → Discard sandbox                                          │
└─────────────────────────────────────────────────────────────────────┘
```

## LLM Provider Flow

### Ollama (Local)

```
User Prompt
     │
     ▼
OllamaAIService
     │
     ├─► Check Health (http://localhost:11434/api/tags)
     │   └─► Model Available? ────No──► Error: "Pull qwen2.5-coder:7b"
     │                          └─Yes
     │
     ├─► Build System Prompt
     │   • Load z3ed-resources.yaml
     │   • Add few-shot examples
     │   • Inject ROM context
     │
     ├─► POST /api/generate
     │   {
     │     "model": "qwen2.5-coder:7b",
     │     "prompt": "<system> + <user>",
     │     "temperature": 0.1,
     │     "format": "json"
     │   }
     │
     ├─► Parse Response
     │   ["command1", "command2", ...]
     │
     └─► Return to Agent Handler
```

### Gemini (Remote)

```
User Prompt
     │
     ▼
GeminiAIService
     │
     ├─► Check API Key
     │   └─► Not Set? ────► Error: "Set GEMINI_API_KEY"
     │
     ├─► Build Request
     │   {
     │     "contents": [{
     │       "role": "user",
     │       "parts": [{"text": "<system> + <prompt>"}]
     │     }],
     │     "generationConfig": {
     │       "temperature": 0.1,
     │       "maxOutputTokens": 2048
     │     }
     │   }
     │
     ├─► POST https://generativelanguage.googleapis.com/
     │         v1beta/models/gemini-1.5-flash:generateContent
     │
     ├─► Parse Response
     │   • Extract text from nested JSON
     │   • Strip markdown code blocks if present
     │   • Parse JSON array
     │
     └─► Return Commands
```

### Claude (Remote)

```
User Prompt
     │
     ▼
ClaudeAIService
     │
     ├─► Check API Key
     │   └─► Not Set? ────► Error: "Set CLAUDE_API_KEY"
     │
     ├─► Build Request
     │   {
     │     "model": "claude-3-5-sonnet-20241022",
     │     "max_tokens": 2048,
     │     "temperature": 0.1,
     │     "system": "<system instructions>",
     │     "messages": [{
     │       "role": "user",
     │       "content": "<prompt>"
     │     }]
     │   }
     │
     ├─► POST https://api.anthropic.com/v1/messages
     │
     ├─► Parse Response
     │   • Extract text from content[0].text
     │   • Strip markdown if present
     │   • Parse JSON array
     │
     └─► Return Commands
```

## Prompt Engineering Pipeline

```
┌─────────────────────────────────────────────────────────────────────┐
│                         PromptBuilder                                │
│                   (Comprehensive System Prompt)                      │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                             ├─► 1. Load Resource Catalogue
                             │   Source: docs/api/z3ed-resources.yaml
                             │   • All command schemas
                             │   • Argument types & descriptions
                             │   • Expected effects & returns
                             │
                             ├─► 2. Add Few-Shot Examples
                             │   Proven prompt → command pairs:
                             │   • "Validate ROM" → ["rom validate ..."]
                             │   • "Red armor" → ["palette export ...", ...]
                             │
                             ├─► 3. Inject ROM Context
                             │   Current state from application:
                             │   • Loaded ROM path
                             │   • Open editors (Overworld, Dungeon)
                             │   • Recently modified assets
                             │
                             ├─► 4. Set Output Format Rules
                             │   • MUST return JSON array of strings
                             │   • Each string is executable z3ed command
                             │   • No explanations or markdown
                             │
                             └─► 5. Combine into Final Prompt
                                 System Prompt (~2K tokens) + User Prompt
                                      │
                                      ▼
                              Sent to LLM Provider
```

## Error Handling & Fallback Chain

```
User Request
     │
     ▼
Select Provider (YAZE_AI_PROVIDER)
     │
     ├─► Ollama Selected
     │   │
     │   ├─► Health Check
     │   │   └─► Failed? ────► Warn + Fallback to MockAIService
     │   │                     "⚠️ Ollama unavailable, using mock"
     │   │
     │   └─► Model Check
     │       └─► Missing? ───► Error + Suggestion
     │                         "Pull model: ollama pull qwen2.5-coder:7b"
     │
     ├─► Gemini Selected
     │   │
     │   ├─► API Key Check
     │   │   └─► Missing? ───► Fallback to MockAIService
     │   │                     "Set GEMINI_API_KEY or use Ollama"
     │   │
     │   └─► API Call
     │       ├─► Network Error? ───► Retry (3x with backoff)
     │       └─► Rate Limit? ──────► Error + Wait Suggestion
     │
     └─► Claude Selected
         │
         └─► Similar to Gemini
             (API key check → Fallback → Retry logic)
```

## File Structure

```
yaze/
├── src/cli/service/
│   ├── ai_service.h              # Base interface
│   ├── ai_service.cc             # MockAIService implementation
│   ├── ollama_ai_service.h       # 🆕 Ollama integration
│   ├── ollama_ai_service.cc      # 🆕 Implementation
│   ├── gemini_ai_service.h       # Existing (needs fixes)
│   ├── gemini_ai_service.cc      # Existing (needs fixes)
│   ├── claude_ai_service.h       # 🆕 Claude integration
│   ├── claude_ai_service.cc      # 🆕 Implementation
│   ├── prompt_builder.h          # 🆕 Prompt engineering utility
│   └── prompt_builder.cc         # 🆕 Implementation
│
├── src/cli/handlers/agent/
│   └── general_commands.cc       # 🔧 Add CreateAIService() factory
│
├── docs/z3ed/
│   ├── LLM-INTEGRATION-PLAN.md           # 📋 Complete guide (this file)
│   ├── LLM-IMPLEMENTATION-CHECKLIST.md   # ✅ Task checklist
│   ├── LLM-INTEGRATION-SUMMARY.md        # 📄 Executive summary
│   ├── LLM-INTEGRATION-ARCHITECTURE.md   # 🏗️ Visual diagrams (this file)
│   └── AI-SERVICE-SETUP.md               # 📖 User guide (future)
│
└── scripts/
    ├── quickstart_ollama.sh      # 🚀 Automated setup test
    └── test_ai_services.sh       # 🧪 Integration tests
```

## Data Flow Example: "Make soldier armor red"

```
1. User Input
   $ z3ed agent run --prompt "Make soldier armor red" --rom zelda3.sfc --sandbox

2. Agent Handler
   • Create proposal (ID: agent_20251003_143022)
   • Create sandbox (/tmp/yaze_sandbox_abc123/zelda3.sfc)
   • Select AI service (Ollama detected)

3. Ollama Service
   • Check health: ✓ Running on localhost:11434
   • Check model: ✓ qwen2.5-coder:7b available
   • Build prompt:
     System: "<full resource catalogue> + <few-shot examples>"
     User: "Make soldier armor red"
   • Call API: POST /api/generate
   • Response: 
     ```json
     {
       "response": "[\"palette export --group sprites --id soldier --to /tmp/soldier.pal\", \"palette set-color --file /tmp/soldier.pal --index 5 --color FF0000\", \"palette import --group sprites --id soldier --from /tmp/soldier.pal\"]"
     }
     ```
   • Parse: Extract 3 commands

4. Command Execution
   ┌────────────────────────────────────────────────────────┐
   │ Command 1: palette export --group sprites --id soldier │
   │ Handler: PaletteHandler::HandleExport()                │
   │ Status: ✓ Success (wrote /tmp/soldier.pal)             │
   │ Duration: 45ms                                          │
   ├────────────────────────────────────────────────────────┤
   │ Command 2: palette set-color --file /tmp/soldier.pal   │
   │ Handler: PaletteHandler::HandleSetColor()              │
   │ Status: ✓ Success (modified index 5 → #FF0000)         │
   │ Duration: 12ms                                          │
   ├────────────────────────────────────────────────────────┤
   │ Command 3: palette import --group sprites --id soldier │
   │ Handler: PaletteHandler::HandleImport()                │
   │ Status: ✓ Success (applied to sandbox ROM)             │
   │ Duration: 78ms                                          │
   └────────────────────────────────────────────────────────┘

5. Proposal Registry
   • Log all commands
   • Calculate ROM diff (before/after)
   • Set status: PENDING_REVIEW

6. Output to User
   ✅ Agent run completed successfully.
      Proposal ID: agent_20251003_143022
      Sandbox: /tmp/yaze_sandbox_abc123/zelda3.sfc
      Use 'z3ed agent diff' to review changes

7. User Review
   $ z3ed agent diff
   
   Proposal: agent_20251003_143022
   Prompt: "Make soldier armor red"
   Status: pending
   Created: 2025-10-03 14:30:22
   
   Executed Commands:
     1. palette export --group sprites --id soldier --to /tmp/soldier.pal
     2. palette set-color --file /tmp/soldier.pal --index 5 --color FF0000
     3. palette import --group sprites --id soldier --from /tmp/soldier.pal
   
   ROM Diff:
     Modified palettes: [sprites/soldier]
     Changed bytes: 6
     Offset 0x12345: [old] 00 7C 00 → [new] 00 00 FF

8. GUI Review
   Open YAZE → Debug → Agent Proposals
   • Visual diff shows red soldier sprite
   • Click "Accept" → Merge sandbox to main ROM
   • Or "Reject" → Discard sandbox

9. Finalization
   $ z3ed agent commit
   ✅ Proposal accepted and merged to zelda3.sfc
```

## Comparison Matrix

| Feature | Ollama | Gemini | Claude | Mock |
|---------|--------|--------|--------|------|
| **Cost** | Free | $0.10/1M tokens | Free tier | Free |
| **Privacy** | ✅ Local | ❌ Remote | ❌ Remote | ✅ Local |
| **Setup** | `brew install` | API key | API key | None |
| **Speed** | Fast (~1-2s) | Medium (~2-4s) | Medium (~2-4s) | Instant |
| **Quality** | Good (7B-70B) | Excellent | Excellent | Hardcoded |
| **Internet** | No | Yes | Yes | No |
| **Rate Limits** | None | 60 req/min | 5 req/min | None |
| **Model Choice** | Many | Fixed | Fixed | N/A |
| **Use Case** | Development | Production | Premium | Testing |

## Next Steps

1. **Read**: [LLM-INTEGRATION-PLAN.md](LLM-INTEGRATION-PLAN.md) for implementation details
2. **Follow**: [LLM-IMPLEMENTATION-CHECKLIST.md](LLM-IMPLEMENTATION-CHECKLIST.md) step-by-step
3. **Test**: Run `./scripts/quickstart_ollama.sh` when ready
4. **Document**: Update this architecture diagram as system evolves

---

**Last Updated**: October 3, 2025  
**Status**: Documentation Complete | Ready to Implement
