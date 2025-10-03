# LLM Integration Implementation Checklist

**Created**: October 3, 2025  
**Status**: Ready to Begin  
**Estimated Time**: 12-15 hours total

> 📋 **Main Guide**: See [LLM-INTEGRATION-PLAN.md](LLM-INTEGRATION-PLAN.md) for detailed implementation instructions.

## Phase 1: Ollama Local Integration (4-6 hours) ✅ COMPLETE

### Prerequisites
- [x] Install Ollama: `brew install ollama` (macOS)
- [x] Start Ollama server: `ollama serve`
- [x] Pull recommended model: `ollama pull qwen2.5-coder:7b`
- [x] Test connectivity: `curl http://localhost:11434/api/tags`

### Implementation Tasks

#### 1.1 Create OllamaAIService Class
- [x] Create `src/cli/service/ollama_ai_service.h`
  - [x] Define `OllamaConfig` struct
  - [x] Declare `OllamaAIService` class with `GetCommands()` override
  - [x] Add `CheckAvailability()` and `ListAvailableModels()` methods
- [x] Create `src/cli/service/ollama_ai_service.cc`
  - [x] Implement constructor with config
  - [x] Implement `BuildSystemPrompt()` with z3ed command documentation
  - [x] Implement `CheckAvailability()` with health check
  - [x] Implement `GetCommands()` with Ollama API call
  - [x] Add JSON parsing for command extraction
  - [x] Add error handling for connection failures

#### 1.2 Update CMake Configuration
- [x] Add `YAZE_WITH_HTTPLIB` option to `CMakeLists.txt`
- [x] Add httplib detection (vcpkg or bundled)
- [x] Add compile definition `YAZE_WITH_HTTPLIB`
- [x] Update z3ed target to link httplib when available

#### 1.3 Wire into Agent Commands
- [x] Update `src/cli/handlers/agent/general_commands.cc`
  - [x] Add `#include "cli/service/ollama_ai_service.h"`
  - [x] Create `CreateAIService()` helper function
  - [x] Implement provider selection logic (env vars)
  - [x] Add health check with fallback to MockAIService
  - [x] Update `HandleRunCommand()` to use service factory
  - [x] Update `HandlePlanCommand()` to use service factory

#### 1.4 Testing & Validation
- [x] Create `scripts/test_ollama_integration.sh`
  - [x] Check Ollama server availability
  - [x] Verify model is pulled
  - [x] Test `z3ed agent run` with simple prompt
  - [x] Verify proposal creation
  - [x] Review generated commands
- [x] Run end-to-end test
- [x] Document any issues encountered

### Success Criteria
- [x] `z3ed agent run --prompt "Validate ROM"` generates correct command
- [x] Health check reports clear errors when Ollama unavailable
- [x] Service fallback to MockAIService works correctly
- [x] Test script passes without manual intervention

**Status:** ✅ Complete - See [PHASE1-COMPLETE.md](PHASE1-COMPLETE.md)

---

## Phase 2: Improve Gemini Integration (2-3 hours) ✅ COMPLETE

### Implementation Tasks

#### 2.1 Fix GeminiAIService
- [x] Update `src/cli/service/gemini_ai_service.h`
  - [x] Add `GeminiConfig` struct with model, temperature, max_tokens
  - [x] Add health check methods
  - [x] Update constructor signature
- [x] Update `src/cli/service/gemini_ai_service.cc`
  - [x] Fix system instruction format (separate field in v1beta API)
  - [x] Update to use `gemini-1.5-flash` model
  - [x] Add generation config (temperature, maxOutputTokens)
  - [x] Add `responseMimeType: application/json` for structured output
  - [x] Implement markdown code block stripping
  - [x] Add `CheckAvailability()` with API key validation
  - [x] Improve error messages with actionable guidance

#### 2.2 Wire into Service Factory
- [x] Update `CreateAIService()` to use `GeminiConfig`
- [x] Add Gemini health check with fallback
- [x] Add `GEMINI_MODEL` environment variable support
- [x] Test with graceful fallback

#### 2.3 Testing
- [x] Create `scripts/test_gemini_integration.sh`
- [x] Test graceful fallback without API key
- [x] Test error handling (invalid key, network issues)
- [ ] Test with real API key (pending)
- [ ] Verify JSON array parsing (pending)
- [ ] Test various prompts (pending)

### Success Criteria
- [x] Gemini service compiles and builds
- [x] Service factory integration works
- [x] Graceful fallback to MockAIService
- [ ] Gemini generates valid command arrays (pending API key)
- [ ] Markdown stripping works reliably (pending API key)
- [x] Error messages guide user to API key setup

**Status:** ✅ Complete (build & integration) - See [PHASE2-COMPLETE.md](PHASE2-COMPLETE.md)  
**Pending:** Real API key validation

---

## Phase 3: Add Claude Integration (2-3 hours)

### Implementation Tasks

#### 3.1 Create ClaudeAIService
- [ ] Create `src/cli/service/claude_ai_service.h`
  - [ ] Define class with API key constructor
  - [ ] Add `GetCommands()` override
- [ ] Create `src/cli/service/claude_ai_service.cc`
  - [ ] Implement Claude Messages API call
  - [ ] Use `claude-3-5-sonnet-20241022` model
  - [ ] Add markdown stripping
  - [ ] Add error handling

#### 3.2 Wire into Service Factory
- [ ] Update `CreateAIService()` to check for `CLAUDE_API_KEY`
- [ ] Add Claude as provider option

#### 3.3 Testing
- [ ] Test with various prompts
- [ ] Compare output quality vs Gemini/Ollama

### Success Criteria
- [ ] Claude service works interchangeably with others
- [ ] Quality comparable or better than Gemini

---

## Phase 4: Enhanced Prompt Engineering (3-4 hours)

### Implementation Tasks

#### 4.1 Create PromptBuilder Utility
- [ ] Create `src/cli/service/prompt_builder.h`
- [ ] Create `src/cli/service/prompt_builder.cc`
  - [ ] Implement `LoadResourceCatalogue()` (read z3ed-resources.yaml)
  - [ ] Implement `BuildSystemPrompt()` with full command docs
  - [ ] Implement `BuildFewShotExamples()` with proven examples
  - [ ] Implement `BuildContextPrompt()` with ROM state

#### 4.2 Integrate into Services
- [ ] Update OllamaAIService to use PromptBuilder
- [ ] Update GeminiAIService to use PromptBuilder
- [ ] Update ClaudeAIService to use PromptBuilder

#### 4.3 Testing
- [ ] Test with complex prompts
- [ ] Measure accuracy improvement
- [ ] Document which models perform best

### Success Criteria
- [ ] System prompts include full resource catalogue
- [ ] Few-shot examples improve accuracy >90%
- [ ] Context injection provides relevant ROM info

---

## Configuration & Documentation

### Environment Variables Setup
- [ ] Document `YAZE_AI_PROVIDER` options
- [ ] Document `OLLAMA_MODEL` override
- [ ] Document API key requirements
- [ ] Create example `.env` file

### User Documentation
- [ ] Create `docs/z3ed/AI-SERVICE-SETUP.md`
  - [ ] Ollama quick start
  - [ ] Gemini setup guide
  - [ ] Claude setup guide
  - [ ] Troubleshooting section
- [ ] Update README with LLM setup instructions
- [ ] Add examples to main docs

### CLI Enhancements
- [ ] Add `--ai-provider` flag to override env
- [ ] Add `--ai-model` flag to override model
- [ ] Add `--dry-run` flag to see commands without executing
- [ ] Add `--interactive` flag to confirm each command

---

## Testing Matrix

| Provider | Model | Test Prompt | Expected Commands | Status |
|----------|-------|-------------|-------------------|--------|
| Ollama | qwen2.5-coder:7b | "Validate ROM" | `["rom validate --rom zelda3.sfc"]` | ⬜ |
| Ollama | codellama:13b | "Export first palette" | `["palette export ..."]` | ⬜ |
| Gemini | gemini-1.5-flash | "Make soldiers red" | `["palette export ...", "palette set-color ...", ...]` | ⬜ |
| Claude | claude-3.5-sonnet | "Change tile at (10,20)" | `["overworld set-tile ..."]` | ⬜ |

---

## Rollout Plan

### Week 1 (Oct 7-11, 2025)
- **Monday**: Phase 1 implementation (OllamaAIService class)
- **Tuesday**: Phase 1 CMake + wiring
- **Wednesday**: Phase 1 testing + documentation
- **Thursday**: Phase 2 (Gemini fixes)
- **Friday**: Buffer day + code review

### Week 2 (Oct 14-18, 2025)
- **Monday**: Phase 3 (Claude integration)
- **Tuesday**: Phase 4 (PromptBuilder)
- **Wednesday**: Enhanced testing across all services
- **Thursday**: Documentation completion
- **Friday**: User validation + demos

---

## Known Risks & Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Ollama not available on CI | Medium | Low | Add `YAZE_AI_PROVIDER=mock` for CI builds |
| LLM output format inconsistent | High | Medium | Strict system prompts + validation layer |
| API rate limits | Medium | Medium | Cache responses, implement retry backoff |
| Model accuracy insufficient | High | Low | Multiple few-shot examples + prompt tuning |

---

## Success Metrics

**Phase 1 Complete**: 
- ✅ Ollama service operational on local machine
- ✅ Can generate valid z3ed commands from prompts
- ✅ End-to-end test passes

**Phase 2-3 Complete**:
- ✅ All three providers (Ollama, Gemini, Claude) work interchangeably
- ✅ Service selection transparent to user

**Phase 4 Complete**:
- ✅ Command accuracy >90% on standard prompts
- ✅ Resource catalogue integrated into system prompts

**Production Ready**:
- ✅ Documentation complete with setup guides
- ✅ Error messages are actionable
- ✅ Works on macOS (primary target)
- ✅ At least one user validates the workflow

---

## Next Steps After Completion

1. **Gather User Feedback**: Share with ROM hacking community
2. **Measure Accuracy**: Track success rate of generated commands
3. **Model Comparison**: Document which models work best
4. **Fine-Tuning**: Consider fine-tuning local models on z3ed corpus
5. **Agentic Loop**: Add self-correction based on execution results

---

## Notes & Observations

_Add notes here as you progress through implementation:_

- 
- 
- 

---

**Last Updated**: October 3, 2025  
**Next Review**: After Phase 1 completion
