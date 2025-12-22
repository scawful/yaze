# Music Editor Development Guide

This is a living document tracking the development of yaze's SNES music editor.

## Overview

The music editor enables editing ALTTP music data stored in ROM, composing new songs, and integrating with the Oracle of Secrets music_macros ASM format for custom music.

## Sprint Plan

### Sprint 1: Data Model
**Status: Complete**

- Core data structures (`Note`, `MusicCommand`, `TrackEvent`, `MusicSong`)
- `MusicBank` manager for ROM loading/saving
- `SpcParser` and `SpcSerializer`
- `BrrCodec` for sample handling

### Sprint 2: Tracker View (Read-Only)
**Status: Complete**

- `TrackerView` component with 8-channel grid
- Song selector and playback integration
- Visualization of notes and commands

### Sprint 3: Tracker Editing
**Status: Complete**

- Keyboard navigation and note entry
- Selection system (cells and ranges)
- Undo/Redo system
- Clipboard placeholder

### Sprint 4: APU Playback
**Status: Complete**

- Mixer Panel with Mute/Solo
- Real-time Volume Meters
- Master Oscilloscope
- DSP state integration

### Sprint 5: Piano Roll
**Status: Complete**

- `PianoRollView` component (Horizontal)
- Mouse-based note entry/editing
- Zoom controls
- Integrated with `MusicEditor` state

### Sprint 6: Instruments & Samples
**Status: Complete**

- `InstrumentEditorView`: ADSR visualization (`ImPlot`), property editing.
- `SampleEditorView`: Waveform visualization (`ImPlot`), loop point editing.
- Basic WAV import placeholder (generates sine wave).
- Integrated into `MusicEditor` tabs.

### Sprint 7: Project Management & Song Browser
**Status: Complete**

- **Song Browser:**
    - Tree view separating "Vanilla Songs" (IDs 1-34) and "Custom Songs" (IDs 35+).
    - Dockable "Song Browser" card in Activity Bar.
    - Search filtering and context menus (Duplicate/Delete).
- **Integration:**
    - Replaced legacy dropdown selector with robust browser selection.
    - Linked `MusicEditor` state to browser actions.

### Sprint 8: Export & Polish
**Status: In Progress**

- ✅ Integrated `SpcParser::ParseSong()` into `MusicBank::LoadSongTable()` so the editor now reflects real ROM song data (vanilla + custom slots).
- [ ] ROM patching finalization (ensure `SaveToRom` works robustly).
- [ ] ASM export (`music_macros` format generation).
- [ ] SPC file export (standalone).
- [ ] Full WAV import/resampling logic (replace dummy sine wave).

---

## Quality Improvements (Review Findings)

Tracked fixes identified during the agent swarm code review.

### High Priority (Blockers)
- [x] **Replace hardcoded colors with theme system** - `tracker_view.cc` (4 instances), `music_editor.cc` (3 instances)
- [x] **Integrate SpcParser into MusicBank** - `MusicBank::LoadSongTable()` now uses `SpcParser::ParseSong()`
- [ ] **Fix serializer relocation bug** - Track address calculation in `SerializeSong` is incorrect
- [ ] **Implement SaveToRom()** - Currently returns `UnimplementedError`

### Medium Priority (Quality)
- [x] **Add undo stack size limit** - Capped at 50 states with FIFO eviction (`music_editor.cc`)
- [x] **Fix oscilloscope ring buffer wrap-around** - Proper mask `& (kBufferSize - 1)` (`music_editor.cc`)
- [ ] **Add VU meter smoothing/peak-hold** - Currently uses instantaneous sample values
- [x] **Change Copy()/Paste() to return UnimplementedError** - Honest API reporting (`music_editor.cc`)

### Low Priority (Nice to Have)
- [ ] Add stereo oscilloscope toggle
- [ ] Implement range deletion in TrackerView
- [ ] Add visual octave indicator (F1/F2 feedback)
- [ ] Per-song undo stacks

### Test Gaps
- [ ] ROM-dependent integration tests (`test/e2e/rom_dependent/music_rom_test.cc`)
- [ ] Error handling tests for parse failures
- [ ] Parse → Serialize → Parse roundtrip validation

---

### Sprint 9: Expanded Music Banks (Oracle of Secrets Integration)
**Status: Planned**

The Oracle of Secrets ROM hack demonstrates expanded music bank support, allowing custom songs beyond vanilla ROM limits. This sprint brings those capabilities to yaze.

**Goals:**
- [ ] Support for expanded bank detection (`SongBank_OverworldExpanded_Main`)
- [ ] Dynamic bank allocation for custom songs (slots 35+)
- [ ] Auxiliary bank support (`SONG_POINTERS_AUX` at `$2B00`)
- [ ] Bank space visualization with overflow warnings
- [ ] Auto-relocate songs when bank space exceeded

**Bank Architecture (from Oracle of Secrets):**

| Symbol | Address | Purpose |
|--------|---------|---------|
| `SPC_ENGINE` | `$0800` | Sound driver code |
| `SFX_DATA` | `$17C0` | Sound effects |
| `SAMPLE_POINTERS` | `$3C00` | Sample directory |
| `INSTRUMENT_DATA` | `$3D00` | Instrument definitions |
| `SAMPLE_DATA` | `$4000` | BRR sample data |
| `SONG_POINTERS` | `$D000` | Main song pointer table |
| `SONG_POINTERS_AUX` | `$2B00` | Auxiliary song pointers |

### Sprint 10: ASM Editor Integration
**Status: Planned**

Full integration with Oracle of Secrets `music_macros.asm` format for bidirectional editing.

**Goals:**
- [ ] Import `.asm` song files directly
- [ ] Export songs to `music_macros` syntax
- [ ] Syntax highlighting for N-SPC commands
- [ ] Live preview compilation (ASM → binary → playback)
- [ ] Subroutine deduplication detection

**Export Format Example:**
```asm
MySong:
!ARAMAddr = $D86A
dw !ARAMAddr+$0A      ; Intro section
dw !ARAMAddr+$1A      ; Looping section
dw $00FF              ; Fade-in
dw !ARAMAddr+$02      ; Loop start
dw $0000

.Channels
!ARAMC = !ARAMAddr-MySong
dw .Channel0+!ARAMC
dw .Channel1+!ARAMC
; ... 8 channels

.Channel0
  %SetMasterVolume($DA)
  %SetTempo(62)
  %Piano()
  %SetDurationN(!4th, $7F)
  %CallSubroutine(.MelodyA+!ARAMC, 3)
  db End
```

### Sprint 11: Common Patterns Library
**Status: Planned**

Inspired by Oracle of Secrets documentation proposals for reusable musical patterns.

**Goals:**
- [ ] Built-in pattern library (drum loops, arpeggios, basslines)
- [ ] User-defined pattern saving/loading
- [ ] Pattern browser with preview
- [ ] Auto-convert repeated sections to subroutines

**Pattern Categories:**
- Percussion: Standard 4/4 beats, fills, breaks
- Basslines: Walking bass, pedal tones, arpeggiated
- Arpeggios: Major, minor, diminished, suspended
- Effects: Risers, sweeps, transitions

---

## Oracle of Secrets Integration

### music_macros.asm Reference

The Oracle of Secrets project uses a comprehensive macro system for music composition. Key macros that yaze should support for import/export:

**Duration Constants:**
| Macro | Value | Description |
|-------|-------|-------------|
| `!4th` | `$48` | Quarter note (72 ticks) |
| `!4thD` | `$6C` | Dotted quarter (108 ticks) |
| `!4thT` | `$30` | Quarter triplet (48 ticks) |

---

## Plain-English Reference (Tracker/ASM)

### Tracker Cell Cheatsheet
- **Tick column**: absolute tick where the event starts (0-based). Quarter note = 72 ticks, eighth = 36, sixteenth = 18.
- **Pitch**: C1 = `$80`, B6 = `$C7`; `$C8` = tie, `$C9` = rest.
- **Duration**: stored per-note; tracker shows the event at the start tick, piano roll shows its full width.
- **Channel**: 8 channels run in parallel; channel-local commands (transpose/volume/pan) affect only that lane.

### Common N-SPC Commands (0xE0–0xFF)
- `$E0 SetInstrument i` — pick instrument index `i` (0–24 vanilla ALTTP).
- `$E1 SetPan p` — pan left/right (`$00` hard L, `$10` center, `$1F` hard R).
- `$E5 SetMasterVolume v` — global volume.
- `$E7 SetTempo t` — tempo; higher = faster.
- `$E9 GlobalTranspose s` — shift all channels by semitones (signed byte).
- `$EA ChannelTranspose s` — shift this channel only.
- `$ED SetChannelVolume v` — per-channel volume.
- `$EF CallSubroutine addr, reps` — jump to a subroutine `reps` times.
- `$F5 EchoVBits mask` / `$F6 EchoOff` — enable/disable echo.
- `$F7 EchoParams vL vR delay` — echo volume/delay.
- `$F9 PitchSlide` — slide toward target pitch.
- `$FA PercussionPatch` — switch to percussion set.
- `$FF` (unused), `$00` marks track end.

### Instruments (ALTTP table at $3D00)
- **Bytes**: `[sample][AD][SR][gain][pitch_hi][pitch_lo]`
  - `sample` = BRR slot (0–24 in vanilla).
  - `AD` = Attack (bits0-3, 0=slow, F=fast) + Decay (bits4-6).
  - `SR` = Sustain rate (bits0-4, higher = faster decay) + Sustain level (bits5-7, 0=quiet, 7=loud).
  - `gain` = raw gain byte (rarely used in ALTTP; ADSR preferred).
  - `pitch_hi/lo` = 16-bit pitch multiplier (0x1000 = normal; >0x1000 raises pitch).
- In the tracker, a `SetInstrument` command changes these fields for the channel until another `$E0` appears.

### music_macros Quick Map
- `%SetInstrument(i)` → `$E0 i`
- `%SetPan(p)` → `$E1 p`
- `%SetTempo(t)` → `$E7 t`
- `%SetChannelVolume(v)` → `$ED v`
- `%CallSubroutine(label, reps)` → `$EF` + 16-bit pointer + `reps`
- `%SetDurationN(!4th, vel)` → duration prefix + note byte; duration constants map to the table above.
- Note names in macros (e.g., `%C4`, `%F#3`) map directly to tracker pitches (C1 = `$80`).

### Durations and Snapping
- Quarter = 72 ticks, Eighth = 36, Sixteenth = 18, Triplets use the `T` constants (e.g., `!4thT` = 48).
- Piano roll snap defaults to sixteenth (18 ticks); turn snap off to place micro-timing.
| `!8th` | `$24` | Eighth note (36 ticks) |
| `!8thD` | `$36` | Dotted eighth (54 ticks) |
| `!8thT` | `$18` | Eighth triplet (24 ticks) |
| `!16th` | `$12` | Sixteenth (18 ticks) |
| `!32nd` | `$09` | Thirty-second (9 ticks) |

**Instrument Helpers:**
```asm
%Piano()      ; SetInstrument($18)
%Strings()    ; SetInstrument($09)
%Trumpet()    ; SetInstrument($11)
%Flute()      ; SetInstrument($16)
%Choir()      ; SetInstrument($15)
%Tympani()    ; SetInstrument($02)
%Harp()       ; SetInstrument($0F)
%Snare()      ; SetInstrument($13)
```

**Note Constants (Octaves 1-6):**
- Format: `C4`, `C4s` (sharp), `D4`, etc.
- Range: `C1` ($80) through `B6` ($C7)
- Special: `Tie` ($C8), `Rest` ($C9), `End` ($00)

### Expanded Bank Hooking

Oracle of Secrets expands music by hooking the `LoadOverworldSongs` routine:

```asm
org $008919 ; LoadOverworldSongs
  JSL LoadOverworldSongsExpanded

LoadOverworldSongsExpanded:
  LDA.w $0FFF : BEQ .light_world
    ; Load expanded bank for Dark World
    LDA.b #SongBank_OverworldExpanded_Main>>0
    STA.b $00
    ; ... set bank pointer
    RTL
  .light_world
    ; Load vanilla bank
    ; ...
```

**yaze Implementation Strategy:**
1. Detect if ROM has expanded music patch applied
2. Parse expanded song table at `SongBank_OverworldExpanded_Main`
3. Allow editing both vanilla and expanded songs
4. Generate proper ASM patches for new songs

### Proposed Advanced Macros

The Oracle of Secrets documentation proposes these macros for cleaner composition (not yet implemented there, but yaze could pioneer):

```asm
; Define a reusable measure
%DefineMeasure(VerseMelody, !8th, C4, D4, E4, F4, G4, A4, B4, C5)
%DefineMeasure(VerseBass, !4th, C2, G2, A2, F2)

; Use in channel data
.Channel0
  %PlayMeasure(VerseMelody, 4)  ; Play 4 times
  db End

.Channel1
  %PlayMeasure(VerseBass, 4)
  db End
```

**Benefits:**
- Channel data reads like a high-level arrangement
- Automatic subroutine address calculation
- Reduced code duplication

---

## Technical Reference

### N-SPC Command Bytes

| Byte | Macro | Params | Description |
|------|-------|--------|-------------|
| $E0 | SetInstrument | 1 | Select instrument (0-24) |
| $E1 | SetPan | 1 | Stereo pan (0-20, 10=center) |
| $E2 | PanFade | 2 | Fade pan over time |
| $E3 | VibratoOn | 3 | Enable pitch vibrato (delay, rate, depth) |
| $E4 | VibratoOff | 0 | Disable vibrato |
| $E5 | SetMasterVolume | 1 | Global volume |
| $E6 | MasterVolumeFade | 2 | Fade master volume |
| $E7 | SetTempo | 1 | Song tempo |
| $E8 | TempoFade | 2 | Gradual tempo change |
| $E9 | GlobalTranspose | 1 | Transpose all channels |
| $EA | ChannelTranspose | 1 | Transpose single channel |
| $EB | TremoloOn | 3 | Volume oscillation |
| $EC | TremoloOff | 0 | Disable tremolo |
| $ED | SetChannelVolume | 1 | Per-channel volume |
| $EE | ChannelVolumeFade | 2 | Fade channel volume |
| $EF | CallSubroutine | 3 | Call music subroutine (addr_lo, addr_hi, repeat) |
| $F0 | VibratoFade | 1 | Fade vibrato depth |
| $F1 | PitchEnvelopeTo | 3 | Pitch slide to note |
| $F2 | PitchEnvelopeFrom | 3 | Pitch slide from note |
| $F3 | PitchEnvelopeOff | 0 | Disable pitch envelope |
| $F4 | Tuning | 1 | Fine pitch adjustment |
| $F5 | EchoVBits | 3 | Echo enable + volumes |
| $F6 | EchoOff | 0 | Disable echo |
| $F7 | EchoParams | 3 | Echo delay/feedback/filter |
| $F8 | EchoVolumeFade | 3 | Fade echo volumes |
| $F9 | PitchSlide | 3 | Direct pitch slide |
| $FA | PercussionPatchBass | 1 | Set percussion base instrument |

### ARAM Layout (Extended)

| Address | Contents |
|---------|----------|
| $0000-$00EF | Zero Page |
| $00F0-$00FF | APU Registers |
| $0100-$01FF | Stack |
| $0200-$07FF | Sound driver code |
| $0800-$17BF | SPC Engine (expanded) |
| $17C0-$2AFF | SFX Data |
| $2B00-$3BFF | Auxiliary song pointers |
| $3C00-$3CFF | Sample pointers |
| $3D00-$3DFF | Instrument definitions |
| $3E00-$3FFF | SFX instrument data |
| $4000-$CFFF | Sample data (~36KB) |
| $D000-$FFBF | Song data (~12KB) |
| $FFC0-$FFFF | IPL ROM |

### Instrument IDs (Extended)

| ID | Name | Notes |
|----|------|-------|
| $00 | Noise | White noise generator |
| $01 | Rain | Rain/ambient |
| $02 | Tympani | Timpani drums |
| $03 | Square wave | 8-bit synth |
| $04 | Saw wave | Sawtooth synth |
| $05 | Sine wave | Pure tone |
| $06 | Wobbly lead | Modulated synth |
| $07 | Compound saw | Rich saw |
| $08 | Tweet | Bird/high chirp |
| $09 | Strings A | Orchestral strings |
| $0A | Strings B | Alternate strings |
| $0B | Trombone | Brass |
| $0C | Cymbal | Crash/ride |
| $0D | Ocarina | Wind instrument |
| $0E | Chime | Bell/chime |
| $0F | Harp | Plucked strings |
| $10 | Splash | Water/impact |
| $11 | Trumpet | Brass lead |
| $12 | Horn | French horn |
| $13 | Snare A | Snare drum |
| $14 | Snare B | Alternate snare |
| $15 | Choir | Vocal pad |
| $16 | Flute | Wind melody |
| $17 | Oof | Voice/grunt |
| $18 | Piano | Keyboard |

---

## Implementation Notes

### ASM Export Strategy

When exporting to `music_macros` format:

1. **Header Generation:**
   - Calculate `!ARAMAddr` based on target bank position
   - Generate intro/loop section pointers
   - Create channel pointer table with `!ARAMC` offsets

2. **Channel Data:**
   - Convert `TrackEvent` objects to macro calls
   - Use instrument helper macros where applicable
   - Apply duration optimization (only emit when changed)

3. **Subroutine Extraction:**
   - Detect repeated note patterns across channels
   - Extract to `.subXXX` labels
   - Replace inline data with `%CallSubroutine()` calls

4. **Naming Convention:**
   - Prefer semantic names: `.MelodyVerseA`, `.BasslineIntro`, `.PercussionFill1`
   - Fall back to numbered: `.sub001`, `.sub002` if semantic unclear

### Bank Space Management

**Constraints:**
- Main bank: ~12KB at `$D000-$FFBF`
- Echo buffer can consume song space if delay > 2
- Subroutines shared across all songs in bank

**Overflow Handling:**
1. Calculate total song size before save
2. Warn user if approaching limit (>90% used)
3. Suggest moving songs to auxiliary bank
4. Auto-suggest subroutine deduplication

---

## References

- [spannerisms ASM Music Guide](https://spannerisms.github.io/asmmusic)
- [Oracle of Secrets GitHub](https://github.com/scawful/Oracle-of-Secrets)
- [SNES APU Documentation](https://wiki.superfamicom.org/spc700-reference)
- Oracle of Secrets `Core/music_macros.asm` (comprehensive macro library)
- Oracle of Secrets `Music/expanded.asm` (bank expansion technique)
- Hyrule Magic source code (tracker.cc)
