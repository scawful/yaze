# Audio Debugging Quick Reference

Quick reference for debugging MusicEditor audio timing issues.

## Audio Timing Checklist

Before investigating audio issues, verify these values are correct:

| Metric | Expected Value | Tolerance |
|--------|----------------|-----------|
| APU cycle rate | 1,024,000 Hz | +/- 1% |
| DSP sample rate | 32,040 Hz | +/- 0.5% |
| Samples per NTSC frame | 533-534 | +/- 2 |
| APU/Master clock ratio | 0.0478 | exact |
| Resampling | 32040 Hz → 48000 Hz | enabled |
| Frame timing | 60.0988 Hz (NTSC) | exact |

## Running Audio Tests

```bash
# Build with ROM tests enabled
cmake --preset mac-dbg \
  -DYAZE_ENABLE_ROM_TESTS=ON \
  -DYAZE_TEST_ROM_PATH=~/zelda3.sfc

cmake --build --preset mac-dbg

# Run all audio tests
ctest --test-dir build -L audio -V

# Run specific test with verbose output
YAZE_TEST_ROM_PATH=~/zelda3.sfc ./build/bin/Debug/yaze_test_rom_dependent \
  --gtest_filter="*AudioTiming*" 2>&1 | tee audio_debug.log

# Generate timing report
./build/bin/Debug/yaze_test_rom_dependent \
  --gtest_filter="*GenerateTimingReport*"
```

## Key Log Categories

Enable these categories for audio debugging:

- `APU` - APU cycle execution
- `APU_TIMING` - Cycle rate diagnostics
- `DSP_TIMING` - Sample generation rates
- `MusicPlayer` - Playback control
- `AudioBackend` - Audio device/resampling

## Common Issues and Fixes

### 1.5x Speed Bug
**Symptom**: Audio plays too fast, sounds pitched up
**Cause**: Missing or incorrect resampling from 32040 Hz to 48000 Hz
**Fix**: Verify `SetAudioStreamResampling(true, 32040, 2)` is called before playback

### Chipmunk Effect
**Symptom**: Audio sounds very high-pitched and fast
**Cause**: Sample rate mismatch - feeding 32kHz data to 48kHz device without resampling
**Fix**: Enable SDL AudioStream resampling or fix sample rate configuration

### Stuttering/Choppy Audio
**Symptom**: Audio breaks up or skips
**Cause**: Buffer underrun - not generating samples fast enough
**Fix**: Check frame timing in `MusicPlayer::Update()`, increase buffer prime size

### Pitch Drift Over Time
**Symptom**: Audio gradually goes out of tune
**Cause**: Floating-point accumulation error in cycle calculation
**Fix**: Use fixed-point ratio in `APU::RunCycles()` (already implemented)

## Critical Code Paths

| File | Function | Purpose |
|------|----------|---------|
| `apu.cc:88-224` | `RunCycles()` | APU/Master clock sync |
| `apu.cc:226-251` | `Cycle()` | DSP tick every 32 cycles |
| `dsp.cc:142-182` | `Cycle()` | Sample generation |
| `dsp.cc:720-846` | `GetSamples()` | Resampling output |
| `music_player.cc:75-156` | `Update()` | Frame timing |
| `music_player.cc:164-236` | `EnsureAudioReady()` | Audio init |
| `audio_backend.cc:359-406` | `SetAudioStreamResampling()` | 32kHz→48kHz |

## Timing Constants

From `apu.cc`:
```cpp
// APU/Master fixed-point ratio (no floating-point drift)
constexpr uint64_t kApuCyclesNumerator = 32040 * 32;      // 1,025,280
constexpr uint64_t kApuCyclesDenominator = 1364 * 262 * 60;  // 21,437,280

// APU cycles per master cycle ≈ 0.0478
// DSP cycles every 32 APU cycles
// Native sample rate: 32040 Hz
// Samples per NTSC frame: 32040 / 60.0988 ≈ 533
```

## Debug Build Flags

Start yaze with debug flags for audio investigation:

```bash
./yaze --debug --log_file=audio_debug.log \
  --rom_file=zelda3.sfc --editor=Music
```

## Test Output Files

Tests write diagnostic files to `/tmp/`:
- `audio_timing_report.txt` - Full timing metrics
- `audio_timing_drift.txt` - Per-second data (CSV format)

Parse CSV data for analysis:
```bash
# Show timing ratios over time
awk -F, 'NR>1 {print $1, $4, $5}' /tmp/audio_timing_drift.txt
```
