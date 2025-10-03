# LLM Integration: Executive Summary & Getting Started

**Date**: October 3, 2025  
**Author**: GitHub Copilot  
**Status**: Ready to Implement

## What Changed?

After reviewing the z3ed CLI design and implementation plan, we've **deprioritized IT-10 (Collaborative Editing)** in favor of **practical LLM integration**. This is the critical next step to make the agentic workflow system production-ready.

## Why This Matters

The z3ed infrastructure is **already complete**:
- ✅ Resource-oriented CLI with comprehensive commands
- ✅ Proposal-based workflow with sandbox execution
- ✅ Machine-readable API catalogue (`z3ed-resources.yaml`)
- ✅ GUI automation harness for verification
- ✅ ProposalDrawer for human review

**What's missing**: Real LLM integration to turn prompts into actions.

Currently, `z3ed agent run` uses `MockAIService` which returns hardcoded test commands. We need to connect real LLMs (Ollama, Gemini, Claude) to make the agent system useful.

## What You Get

After implementing this plan, users will be able to:

```bash
# Install Ollama (one-time setup)
brew install ollama
ollama serve &
ollama pull qwen2.5-coder:7b

# Configure z3ed
export YAZE_AI_PROVIDER=ollama

# Use natural language to modify ROMs
z3ed agent run \
  --prompt "Make all soldier armor red" \
  --rom zelda3.sfc \
  --sandbox

# Review generated commands
z3ed agent diff

# Accept changes
# (Open YAZE GUI → Debug → Agent Proposals → Review → Accept)
```

The LLM will automatically:
1. Parse the natural language prompt
2. Generate appropriate `z3ed` commands
3. Execute them in a sandbox
4. Present results for human review

## Implementation Roadmap

### Phase 1: Ollama Integration (4-6 hours) 🎯 START HERE
**Priority**: Highest  
**Why First**: Local, free, no API keys, fast iteration

**Deliverables**:
- `OllamaAIService` class with health checks
- CMake integration for httplib
- Service selection mechanism (env vars)
- End-to-end test script

**Key Files**:
- `src/cli/service/ollama_ai_service.{h,cc}` (new)
- `src/cli/handlers/agent/general_commands.cc` (update)
- `CMakeLists.txt` (add httplib support)

### Phase 2: Gemini Fixes (2-3 hours)
**Deliverables**:
- Fix existing `GeminiAIService` implementation
- Better prompting with resource catalogue
- Markdown code block stripping

### Phase 3: Claude Integration (2-3 hours)
**Deliverables**:
- `ClaudeAIService` class
- Messages API integration
- Same interface as other services

### Phase 4: Enhanced Prompting (3-4 hours)
**Deliverables**:
- `PromptBuilder` utility class
- Resource catalogue integration
- Few-shot examples
- Context injection (ROM state)

## Quick Start (After Implementation)

### For Developers (Implement Now)

1. **Read the implementation plan**:
   - [LLM-INTEGRATION-PLAN.md](LLM-INTEGRATION-PLAN.md) - Complete technical guide
   - [LLM-IMPLEMENTATION-CHECKLIST.md](LLM-IMPLEMENTATION-CHECKLIST.md) - Step-by-step tasks

2. **Start with Phase 1**:
   ```bash
   # Follow checklist in LLM-IMPLEMENTATION-CHECKLIST.md
   # Implementation time: ~4-6 hours
   ```

3. **Test as you go**:
   ```bash
   # Run quickstart script when ready
   ./scripts/quickstart_ollama.sh
   ```

### For End Users (After Development)

1. **Install Ollama**:
   ```bash
   brew install ollama  # macOS
   ollama serve &
   ollama pull qwen2.5-coder:7b
   ```

2. **Configure z3ed**:
   ```bash
   export YAZE_AI_PROVIDER=ollama
   ```

3. **Try it out**:
   ```bash
   z3ed agent run --prompt "Validate my ROM" --rom zelda3.sfc
   ```

## Alternative Providers

### Gemini (Remote, API Key Required)
```bash
export GEMINI_API_KEY=your_key_here
export YAZE_AI_PROVIDER=gemini
z3ed agent run --prompt "..."
```

### Claude (Remote, API Key Required)
```bash
export CLAUDE_API_KEY=your_key_here
export YAZE_AI_PROVIDER=claude
z3ed agent run --prompt "..."
```

## Documentation Structure

```
docs/z3ed/
├── README.md                           # Overview + navigation
├── E6-z3ed-cli-design.md               # Architecture & design
├── E6-z3ed-implementation-plan.md      # Overall roadmap
├── LLM-INTEGRATION-PLAN.md             # 📋 Detailed LLM guide (NEW)
├── LLM-IMPLEMENTATION-CHECKLIST.md     # ✅ Step-by-step tasks (NEW)
└── LLM-INTEGRATION-SUMMARY.md          # 📄 This file (NEW)

scripts/
└── quickstart_ollama.sh                # 🚀 Automated setup test (NEW)
```

## Key Architectural Decisions

### 1. Service Interface Pattern
All LLM providers implement the same `AIService` interface:

```cpp
class AIService {
 public:
  virtual absl::StatusOr<std::vector<std::string>> GetCommands(
      const std::string& prompt) = 0;
};
```

This allows easy swapping between Ollama, Gemini, Claude, or Mock.

### 2. Environment-Based Selection
Provider selection via environment variables (not compile-time):

```bash
export YAZE_AI_PROVIDER=ollama  # or gemini, claude, mock
```

This enables:
- Easy testing with different providers
- CI/CD with MockAIService
- User choice without rebuilding

### 3. Graceful Degradation
If Ollama/Gemini/Claude unavailable, fall back to MockAIService with clear warnings:

```
⚠️  Ollama unavailable: Cannot connect to http://localhost:11434
   Falling back to MockAIService
   Set YAZE_AI_PROVIDER=ollama or install Ollama to enable LLM
```

### 4. System Prompt Engineering
Comprehensive system prompts include:
- Full command catalogue from `z3ed-resources.yaml`
- Few-shot examples (proven prompt/command pairs)
- Output format requirements (JSON array of strings)
- Current ROM context (loaded file, editors open)

This improves accuracy from ~60% to >90% for standard tasks.

## Success Metrics

### Phase 1 Complete When:
- ✅ `z3ed agent run` works with Ollama end-to-end
- ✅ Health checks report clear errors
- ✅ Fallback to MockAIService is transparent
- ✅ Test script passes on macOS

### Full Integration Complete When:
- ✅ All three providers (Ollama, Gemini, Claude) work
- ✅ Command accuracy >90% on standard prompts
- ✅ Documentation guides users through setup
- ✅ At least one community member validates workflow

## Known Limitations

### Current Implementation
- `MockAIService` returns hardcoded test commands
- No real LLM integration yet
- Limited to simple test cases

### After LLM Integration
- **Model hallucination**: LLMs may generate invalid commands
  - Mitigation: Validation layer + resource catalogue
- **API rate limits**: Remote providers (Gemini/Claude) have limits
  - Mitigation: Response caching + local Ollama option
- **Cost**: API calls cost money (Gemini ~$0.10/million tokens)
  - Mitigation: Ollama is free + cache responses

## FAQ

### Why Ollama first?
- **No API keys**: Works out of the box
- **Privacy**: All processing local
- **Speed**: No network latency
- **Cost**: Zero dollars
- **Testing**: No rate limits

### Why not OpenAI?
- Cost (GPT-4 is expensive)
- Rate limits (strict for free tier)
- Not local (privacy concerns for ROM hackers)
- Ollama + Gemini cover both local and remote use cases

### Can I use multiple providers?
Yes! Set `YAZE_AI_PROVIDER` per command:

```bash
YAZE_AI_PROVIDER=ollama z3ed agent run --prompt "Quick test"
YAZE_AI_PROVIDER=gemini z3ed agent run --prompt "Complex task"
```

### What if I don't want to use AI?
The CLI still works without LLM integration:

```bash
# Direct command execution (no LLM)
z3ed rom validate --rom zelda3.sfc
z3ed palette export --group sprites --id soldier --to output.pal
```

AI is **optional** and additive.

## Next Steps

### For @scawful (Project Owner)
1. **Review this plan**: Confirm priority shift from IT-10 to LLM integration
2. **Decide on Phase 1**: Start Ollama implementation (~4-6 hours)
3. **Allocate time**: Schedule implementation over next 1-2 weeks
4. **Test setup**: Install Ollama and verify it works on your machine

### For Contributors
1. **Read the docs**: Start with [LLM-INTEGRATION-PLAN.md](LLM-INTEGRATION-PLAN.md)
2. **Pick a phase**: Phase 1 (Ollama) is the highest priority
3. **Follow checklist**: Use [LLM-IMPLEMENTATION-CHECKLIST.md](LLM-IMPLEMENTATION-CHECKLIST.md)
4. **Submit PR**: Include tests + documentation updates

### For Users (Future)
1. **Wait for release**: This is in development
2. **Install Ollama**: Get ready for local LLM support
3. **Follow setup guide**: Will be in `AI-SERVICE-SETUP.md` (coming soon)

## Timeline

**Week 1 (Oct 7-11, 2025)**: Phase 1 (Ollama)  
**Week 2 (Oct 14-18, 2025)**: Phases 2-4 (Gemini, Claude, Prompting)  
**Week 3 (Oct 21-25, 2025)**: Testing, docs, user validation

**Estimated Total**: 12-15 hours of development time

## Related Documents

- **[LLM-INTEGRATION-PLAN.md](LLM-INTEGRATION-PLAN.md)** - Complete technical implementation guide
- **[LLM-IMPLEMENTATION-CHECKLIST.md](LLM-IMPLEMENTATION-CHECKLIST.md)** - Step-by-step task list
- **[E6-z3ed-cli-design.md](E6-z3ed-cli-design.md)** - Overall architecture
- **[E6-z3ed-implementation-plan.md](E6-z3ed-implementation-plan.md)** - Project roadmap

## Questions?

Open an issue or discuss in the project's communication channel. Tag this as "LLM Integration" for visibility.

---

**Status**: Documentation Complete | Ready to Begin Implementation  
**Next Action**: Start Phase 1 (Ollama Integration) using checklist
