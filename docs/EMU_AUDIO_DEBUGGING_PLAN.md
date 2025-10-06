# Emulator Audio and Debugging Integration Plan (SPC700/DSP, MusicEditor, z3ed)

## Goals
- Restore accuracy, stability, and usability of SPC700 and S-DSP for MusicEditor playback and z3ed workflows.
- Provide debugger-grade introspection and control across CPU/SPC/DSP for ROM hack iteration loops.
- Enable AI-assisted debugging: breakpoint suggestions, state capture/compare, and analysis surfaced in CLI (z3ed) and GUI.

## Current State (observed)
- SPC700 core: full instruction table implemented; reset path executes IPL sequence via APU callbacks.
- S-DSP: BRR decode, ADSR/GAIN, noise, echo/FIR, per-channel mixing present. Ring-buffer resampling interface exposed by `Dsp::GetSamples`.
- APU: memory-mapped I/O implemented, timers, DSP ports ($F2/$F3), boot ROM mirroring, SPC700/Apu glue.
- SNES timing: APU catch-up hooked at frame boundary; GUI `Emulator` queues SDL audio from `Snes::SetSamples`.
- Editor/CLI: MusicEditor skeleton; z3ed has music listing/info commands but not wired to live emulation.
- Tests: No SPC/DSP unit tests; emulator code compiles as part of core lib and test runner.

## Gaps and Issues
- DSP sample ring-buffer/readout mismatch: inconsistent field names and resampler cursor produced silent/incorrect audio and potential OOB access. (Fixed below.)
- No unit tests protect APU/DSP registers, timers, or audio frame generation.
- No SPC700 stepping/tests; limited confidence on edge cases (e.g., reset/idle timings/timers sync).
- Debugger integration missing for APU: no SPC breakpoints, no port monitor, no state snapshots.
- z3ed lacks live emulator control surface and automation hooks for debugger/trace playback.

## Architecture Improvements
- Audio correctness
  - Confirm native sample cadence (NTSC ≈ 534/stereo frame; PAL ≈ 641) and ring-buffer alignment at `NewFrame` boundaries.
  - Verify FLG/mute/echo writeback behavior and ENDx semantics; clamp paths for FIR and feedback.
  - Expose APU audio sink abstraction for deterministic capture (tests and CLI recording).
- Deterministic timing
  - Ensure `Snes::CatchUpApu` and `apu.RunCycles` keep SPC timers synchronized with master cycles, including DMA and vblank edges.
  - Add a lightweight "headless" audio stepper: run N CPU cycles, then read M audio samples.
- Debugger substrate
  - Add SPC breakpoints (PC/addr R/W), instruction log buffer, and port watchlist ($F0-$FF, $F2/$F3).
  - Snapshot/restore for SPC/DSP/ARAM; small diffs for AI analysis.
  - Structured trace events (JSON): opcode, operands, flags, ports, timers, ENDx, KOn/KOf.

## z3ed Integration
- Commands (phase 1)
  - `emu spc run/step/reset` (with cycle or opcode granularity)
  - `emu spc break --pc 0x...` and `emu spc rb/wb --addr/len`
  - `emu dsp peek/poke --reg 0x.. --val 0x..` and `emu dsp frame --samples N --out wav|raw`
  - `emu apu timers` (read/clear $FD-$FF), `emu apu ports` (in/out port dumps)
- Commands (phase 2)
  - `music play --id <track>` to patch/workflow-load song state and run SPC script
  - `emu snapshot save/load --scope spc|dsp|apu|all`
  - `emu trace start/stop --filters` produce JSONL for AI

## MusicEditor Integration
- Provide a "Live APU" mode that feeds keystroke notes/instrument edits into ARAM/DSP registers directly.
- Add mini-Audio Monitor: per-voice ENVX/OUTX, KOn/KOf, SRCN, pitch, echo levels.
- Optional: render echo/FIR tap meters.

## Easy Wins (implemented/prioritized)
- FIX: DSP ring-buffer naming and resampling cursor alignment for `GetSamples`.
- TESTS: APU DSP port R/W smoke test; timers increment/reset semantics; `GetSamples` returns silence post-reset.
- CLI scaffolding: extend `music-*` handlers later to call emulator control (future PR).

## Next Steps (high-impact)
1. Add SPC instruction log/callbacks to existing CPU log UI; expose to z3ed.
2. Implement SPC breakpoints and memory watchpoints; expose to CLI.
3. Add snapshot/restore of SPC700/DSP/ARAM.
4. Add WAV dump command for quick audio comparison and regression testing.
5. Write golden tests for a few known SPC sequences (e.g., key-on ADSR envelope progression).

## Test Plan
- Unit
  - APU DSP port R/W mirrors state; master/echo volumes settable/readable.
  - Timers: enable/target; counter increment cadence; read clears.
  - Audio: `GetSamples` returns ring-buffer data; silence post-reset.
- Integration
  - CPU frame loop produces consistent `NewFrame` calls; PAL/NTSC sample counts respected.
  - Round-trip z3ed command sets/reads registers and returns structured outputs.

## Milestones
- M1 (this change): ring-buffer fix + unit tests.
- M2: SPC breakpoints/log + CLI control.
- M3: Snapshot/trace + AI analyzer glue.
- M4: MusicEditor live APU monitor and keyboard input-to-note injection.
