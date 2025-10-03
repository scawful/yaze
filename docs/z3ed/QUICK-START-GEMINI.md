# Quick Start: Gemini AI Integration

**Date**: October 3, 2025  
**Status**: ✅ Ready to Test

## 🚀 Immediate Steps

### 1. Build z3ed with SSL Support

```bash
cd /Users/scawful/Code/yaze

# Build z3ed (SSL is now enabled)
cmake --build build-grpc-test --target z3ed

# Verify OpenSSL is linked
otool -L build-grpc-test/bin/z3ed | grep -i ssl

# Expected output:
#   /opt/homebrew/Cellar/openssl@3/3.5.4/lib/libssl.3.dylib
#   /opt/homebrew/Cellar/openssl@3/3.5.4/lib/libcrypto.3.dylib
```

### 2. Set Up Gemini API Key

**Get Your API Key**:
1. Go to https://aistudio.google.com/apikey
2. Sign in with Google account
3. Click "Create API Key"
4. Copy the key (starts with `AIza...`)

**Set Environment Variable**:
```bash
export GEMINI_API_KEY="AIzaSy..."

# Or add to your ~/.zshrc for persistence:
echo 'export GEMINI_API_KEY="AIzaSy..."' >> ~/.zshrc
source ~/.zshrc
```

### 3. Test Basic Connection

```bash
# Simple test prompt
./build-grpc-test/bin/z3ed agent plan --prompt "Place a tree at position 10, 10"

# Expected output:
# ✓ Using Gemini AI service
# ✓ Commands generated:
#   overworld set-tile --map 0 --x 10 --y 10 --tile 0x02E
```

## 📝 Example Prompts to Try

### Overworld Tile16 Editing

**Single Tile Placement**:
```bash
./build-grpc-test/bin/z3ed agent plan --prompt "Place a tree at position 10, 20 on map 0"
./build-grpc-test/bin/z3ed agent plan --prompt "Add a rock at coordinates 15, 8"
./build-grpc-test/bin/z3ed agent plan --prompt "Put a bush at 5, 5"
```

**Area Creation**:
```bash
./build-grpc-test/bin/z3ed agent plan --prompt "Create a 3x3 water pond at coordinates 15, 10"
./build-grpc-test/bin/z3ed agent plan --prompt "Make a 2x4 dirt patch at 20, 15"
```

**Path/Line Creation**:
```bash
./build-grpc-test/bin/z3ed agent plan --prompt "Add a dirt path from position 5,5 to 5,15"
./build-grpc-test/bin/z3ed agent plan --prompt "Create a horizontal stone path at y=10 from x=8 to x=20"
```

**Pattern Creation**:
```bash
./build-grpc-test/bin/z3ed agent plan --prompt "Plant a row of trees horizontally at y=8 from x=20 to x=25"
./build-grpc-test/bin/z3ed agent plan --prompt "Add trees in a circle around position 30, 30"
```

### Dungeon Editing (Label-Aware)

```bash
./build-grpc-test/bin/z3ed agent plan --prompt "Add 3 soldiers to the Eastern Palace entrance room"
./build-grpc-test/bin/z3ed agent plan --prompt "Place a chest in Hyrule Castle treasure room"
./build-grpc-test/bin/z3ed agent plan --prompt "Add a key to room 0x10 in dungeon 0x02"
```

## 🔍 What to Look For

### Good AI Response Example:
```json
{
  "commands": [
    "overworld set-tile --map 0 --x 10 --y 10 --tile 0x02E"
  ],
  "reasoning": "Placing tree tile (0x02E) at specified coordinates"
}
```

### Quality Checks:
- ✅ AI uses correct tile16 IDs (0x02E for trees, 0x022 for dirt, etc.)
- ✅ AI explains what it's doing
- ✅ Commands follow correct syntax
- ✅ AI handles edge cases (water borders, path curves)
- ✅ AI suggests reasonable positions

## 🐛 Troubleshooting

### Error: "Cannot reach Gemini API"
**Causes**:
- No internet connection
- Incorrect API key
- SSL not enabled

**Solutions**:
```bash
# Verify internet
ping -c 3 google.com

# Verify API key is set
echo $GEMINI_API_KEY

# Verify SSL is linked
otool -L build-grpc-test/bin/z3ed | grep ssl
```

### Error: "Invalid Gemini API key"
**Causes**:
- Typo in API key
- API key not activated
- Rate limit exceeded

**Solutions**:
1. Verify key at https://aistudio.google.com/apikey
2. Generate a new key if needed
3. Wait a few minutes if rate-limited

### Error: "No valid commands extracted"
**Causes**:
- AI didn't understand prompt
- Prompt too vague
- AI output format incorrect

**Solutions**:
1. Rephrase prompt more clearly
2. Use examples from this guide
3. Check logs: `./build-grpc-test/bin/z3ed agent plan --prompt "..." -v`

## 📊 Command Reference

### Tile16 Reference (Common IDs)

| Tile | ID (Hex) | Description |
|------|----------|-------------|
| Grass | 0x020 | Standard grass tile |
| Dirt | 0x022 | Dirt/path tile |
| Tree | 0x02E | Full tree tile |
| Bush | 0x003 | Bush tile |
| Rock | 0x004 | Rock tile |
| Flower | 0x021 | Flower tile |
| Sand | 0x023 | Desert sand |
| Water (top) | 0x14C | Water top edge |
| Water (middle) | 0x14D | Water middle |
| Water (bottom) | 0x14E | Water bottom edge |

### Map IDs

| Map | ID | Description |
|-----|-----|-------------|
| Light World | 0 | Main overworld |
| Dark World | 1 | Dark world version |
| Desert | 3 | Desert area |

### Dungeon IDs

| Dungeon | ID (Hex) | Description |
|---------|----------|-------------|
| Hyrule Castle | 0x00 | Starting castle |
| Eastern Palace | 0x02 | First dungeon |
| Desert Palace | 0x04 | Second dungeon |
| Tower of Hera | 0x07 | Third dungeon |

## 🔧 Advanced Usage

### Full Workflow (with Sandbox)

```bash
# Generate proposal with sandbox isolation
./build-grpc-test/bin/z3ed agent run \
  --prompt "Create a water pond at 15, 10" \
  --rom assets/zelda3.sfc \
  --sandbox

# This will:
# 1. Create sandbox ROM copy
# 2. Generate AI commands
# 3. Apply to sandbox
# 4. Save diff
# 5. Keep original ROM untouched
```

### Batch Testing

```bash
# Create test script
cat > test_prompts.sh << 'EOF'
#!/bin/bash
PROMPTS=(
  "Place a tree at 10, 10"
  "Create a water pond at 15, 20"
  "Add a dirt path from 5,5 to 5,15"
  "Plant trees horizontally at y=8"
)

for prompt in "${PROMPTS[@]}"; do
  echo "Testing: $prompt"
  ./build-grpc-test/bin/z3ed agent plan --prompt "$prompt"
  echo "---"
done
EOF

chmod +x test_prompts.sh
./test_prompts.sh
```

### Logging for Debugging

```bash
# Enable verbose logging
./build-grpc-test/bin/z3ed agent plan \
  --prompt "test" \
  --log-level debug \
  2>&1 | tee gemini_test.log

# Check what AI returned
cat gemini_test.log | grep -A 10 "AI Response"
```

## 📈 Success Metrics

After testing, verify:

### Technical Success
- [ ] Binary has OpenSSL linked (`otool -L` shows libssl/libcrypto)
- [ ] Gemini API responds (no connection errors)
- [ ] Commands are well-formed (correct syntax)
- [ ] Tile16 IDs are correct (match reference table)

### Quality Success
- [ ] AI understands natural language prompts
- [ ] AI explains its reasoning
- [ ] AI handles edge cases (pond edges, path curves)
- [ ] AI suggests reasonable coordinates

### User Experience Success
- [ ] Prompts feel natural to write
- [ ] Responses are easy to understand
- [ ] Commands work when executed
- [ ] Errors are informative

## 🎯 Next Steps

Once testing is successful:

1. **Document Results**:
   - Update `TESTING-SESSION-RESULTS.md`
   - Note any issues or improvements needed
   - Share example outputs

2. **Begin Phase 2 Implementation**:
   - Create `Tile16ProposalGenerator` class
   - Implement proposal JSON format
   - Add CLI commands for overworld editing

3. **Iterate on Prompts**:
   - Add more few-shot examples based on testing
   - Refine tile16 reference for AI
   - Document common failure patterns

## 📞 Support

### Documentation References
- `docs/z3ed/SSL-AND-COLLABORATIVE-PLAN.md` - SSL implementation details
- `docs/z3ed/OVERWORLD-DUNGEON-AI-PLAN.md` - Strategic roadmap
- `docs/z3ed/SESSION-SUMMARY-OCT3-2025.md` - Full session summary
- `docs/z3ed/AGENTIC-PLAN-STATUS.md` - Overall project status

### Common Issues
- **SSL Errors**: Check OpenSSL is linked, try rebuilding
- **API Key Issues**: Verify at aistudio.google.com
- **Command Errors**: Review prompt examples, use more specific language
- **Rate Limits**: Wait 1 minute between large batches

---

**Quick Test Command** (Copy/Paste Ready):
```bash
export GEMINI_API_KEY="your-key-here" && \
./build-grpc-test/bin/z3ed agent plan \
  --prompt "Place a tree at position 10, 10"
```

**Status**: ✅ READY TO TEST  
**Next**: Build, test, iterate!

