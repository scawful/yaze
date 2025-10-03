# Testing Gemini Integration

You mentioned you've set up `GEMINI_API_KEY` in your environment with billing enabled. Here's how to test it:

## Quick Test

Open your terminal and run:

```bash
# Make sure the API key is exported
export GEMINI_API_KEY='your-api-key-here'

# Run the manual test script
./scripts/manual_gemini_test.sh
```

Or run it in one line:

```bash
GEMINI_API_KEY='your-api-key' ./scripts/manual_gemini_test.sh
```

## Individual Command Tests

Test individual commands:

```bash
# Export the key first
export GEMINI_API_KEY='your-api-key-here'

# Test 1: Simple palette change
./build/bin/z3ed agent plan --prompt "Change palette 0 color 5 to red"

# Test 2: Overworld modification
./build/bin/z3ed agent plan --prompt "Place a tree at position (10, 20) on map 0"

# Test 3: Multi-step task
./build/bin/z3ed agent plan --prompt "Export palette 0, change color 3 to blue, and import it back"

# Test 4: Create a proposal
./build/bin/z3ed agent run --prompt "Validate the ROM"
```

## What to Look For

1. **Service Selection**: Should say "🤖 Using Gemini AI with model: gemini-2.5-flash"
2. **Command Generation**: Should output a list of z3ed commands like:
   ```
   AI Agent Plan:
     - palette export --group overworld --id 0 --to palette.json
     - palette set-color --file palette.json --index 5 --color 0xFF0000
   ```
3. **No "z3ed" Prefix**: Commands should NOT start with "z3ed" (our parser strips it)
4. **Valid Syntax**: Commands should match the z3ed command syntax

## Expected Output Example

```
🤖 Using Gemini AI with model: gemini-2.5-flash
AI Agent Plan:
  - palette export --group overworld --id 0 --to palette.json
  - palette set-color --file palette.json --index 5 --color 0xFF0000
  - palette import --group overworld --id 0 --from palette.json
```

## Troubleshooting

**Issue**: "Using MockAIService (no LLM configured)"
- **Solution**: Make sure `GEMINI_API_KEY` is exported: `export GEMINI_API_KEY='your-key'`

**Issue**: "Invalid Gemini API key"
- **Solution**: Verify your key at https://makersuite.google.com/app/apikey

**Issue**: "Cannot reach Gemini API"
- **Solution**: Check your internet connection

**Issue**: Commands have "z3ed" prefix
- **Solution**: This is normal - our parser automatically strips it

## Running the Full Test Suite

Once your key is exported, run:

```bash
./scripts/test_gemini_integration.sh
```

This runs 10 comprehensive tests including:
- API connectivity
- Model availability
- Command generation
- Error handling
- Environment variable support

## What We're Testing

This validates Phase 2 implementation:
- ✅ Gemini v1beta API integration
- ✅ JSON response parsing
- ✅ Markdown stripping (if model wraps in ```json)
- ✅ Health check system
- ✅ Error handling
- ✅ Service factory selection

## After Testing

Please share:
1. Did all tests pass? ✅
2. Quality of generated commands (accurate/reasonable)?
3. Response time (fast/slow)?
4. Any errors or issues?

This will help us document Phase 2 completion and decide next steps!
