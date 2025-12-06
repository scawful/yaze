#include "app/editor/music/music_player.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "app/emu/emulator.h"
#include "app/emu/audio/audio_backend.h"
#include "zelda3/music/spc_serializer.h"
#include "zelda3/music/music_bank.h"
#include "util/log.h"

namespace yaze {
namespace editor {
namespace music {

constexpr int kNativeSampleRate = 32040;  // Actual SPC700 rate


MusicPlayer::MusicPlayer(zelda3::music::MusicBank* music_bank)
    : music_bank_(music_bank) {}

MusicPlayer::~MusicPlayer() {
  Stop();
}

void MusicPlayer::SetEmulator(emu::Emulator* emulator) {
  emulator_ = emulator;
}

void MusicPlayer::SetRom(Rom* rom) {
  rom_ = rom;
}

PlaybackState MusicPlayer::GetState() const {
  PlaybackState state;
  state.is_playing = (mode_ == PlaybackMode::Playing || mode_ == PlaybackMode::Previewing);
  state.is_paused = (mode_ == PlaybackMode::Paused);
  state.playing_song_index = playing_song_index_;
  state.current_tick = GetCurrentPlaybackTick();
  state.current_segment_index = playback_segment_index_;
  state.playback_speed = 1.0f;  // Always 1.0x - varispeed removed
  state.ticks_per_second = ticks_per_second_;
  return state;
}

void MusicPlayer::TransitionTo(PlaybackMode new_mode) {
  if (mode_ == new_mode) return;

  PlaybackMode old_mode = mode_;
  mode_ = new_mode;

  // Notify external systems about audio exclusivity changes
  // When we start playing, request exclusive audio control
  // When we stop, release it
  if (audio_exclusivity_callback_) {
    bool was_active = (old_mode == PlaybackMode::Playing || old_mode == PlaybackMode::Previewing);
    bool is_active = (new_mode == PlaybackMode::Playing || new_mode == PlaybackMode::Previewing);

    if (is_active && !was_active) {
      LOG_INFO("MusicPlayer", "Requesting exclusive audio control");
      audio_exclusivity_callback_(true);
    } else if (!is_active && was_active) {
      LOG_INFO("MusicPlayer", "Releasing exclusive audio control");
      audio_exclusivity_callback_(false);
    }
  }

  LOG_DEBUG("MusicPlayer", "State transition: %d -> %d",
            static_cast<int>(old_mode), static_cast<int>(new_mode));
}

void MusicPlayer::PrepareAudioPlayback() {
  if (!emulator_) {
    LOG_ERROR("MusicPlayer", "PrepareAudioPlayback: No emulator");
    return;
  }

  auto* audio = emulator_->audio_backend();
  if (!audio) {
    LOG_ERROR("MusicPlayer", "PrepareAudioPlayback: No audio backend");
    return;
  }

  // TRACE: Log audio pipeline state before preparation
  auto config = audio->GetConfig();
  LOG_INFO("MusicPlayer", "PrepareAudioPlayback: backend=%s, device_rate=%dHz, "
           "resampling=%s, native_rate=%dHz",
           audio->GetBackendName().c_str(), config.sample_rate,
           audio->IsAudioStreamEnabled() ? "ENABLED" : "DISABLED",
           kNativeSampleRate);

  // Reset DSP sample buffer for clean start
  auto& dsp = emulator_->snes().apu().dsp();
  dsp.ResetSampleBuffer();

  // Run one audio frame to generate initial samples
  emulator_->snes().RunAudioFrame();

  // Reset frame timing to prevent accumulated time from causing fast playback
  emulator_->ResetFrameTiming();

  // Queue initial samples to prime the audio buffer
  constexpr int kInitialSamples = 533;  // ~1 frame worth
  static int16_t prime_buffer[2048];
  std::memset(prime_buffer, 0, sizeof(prime_buffer));
  emulator_->snes().SetSamples(prime_buffer, kInitialSamples);

  bool queued = audio->QueueSamplesNative(prime_buffer, kInitialSamples, 2, kNativeSampleRate);

  // TRACE: Log queue result and verify resampling is still active
  LOG_INFO("MusicPlayer", "PrepareAudioPlayback: queued=%s, samples=%d, "
           "resampling_after=%s",
           queued ? "YES" : "NO", kInitialSamples,
           audio->IsAudioStreamEnabled() ? "ENABLED" : "DISABLED");

  if (!queued) {
    LOG_ERROR("MusicPlayer", "PrepareAudioPlayback: CRITICAL - Failed to queue samples! "
              "Audio will not play correctly.");
  }

  audio->Play();

  // Enable audio-focused mode and start emulator
  emulator_->set_audio_focus_mode(true);
  emulator_->set_running(true);

  // Initialize frame timing for Update() loop - CRITICAL for correct playback speed
  last_frame_time_ = std::chrono::steady_clock::now();
}

void MusicPlayer::TogglePlayPause() {
  switch (mode_) {
    case PlaybackMode::Playing:
    case PlaybackMode::Previewing:
      Pause();
      break;
    case PlaybackMode::Paused:
      Resume();
      break;
    case PlaybackMode::Stopped:
      if (playing_song_index_ >= 0) {
        PlaySong(playing_song_index_);
      }
      break;
  }
}

void MusicPlayer::Update() {
  // DIAGNOSTIC: Log Update() entry to verify it's being called
  static int update_count = 0;
  static bool first_update = true;
  if (first_update || update_count % 300 == 0) {
    LOG_INFO("MusicPlayer", "Update() #%d: emu=%p, init=%d, running=%d, focus=%d",
             update_count,
             static_cast<void*>(emulator_),
             emulator_ ? emulator_->is_snes_initialized() : false,
             emulator_ ? emulator_->running() : false,
             emulator_ ? emulator_->is_audio_focus_mode() : false);
    first_update = false;
  }
  update_count++;

  // Run audio frame if we're playing and have an initialized emulator
  if (emulator_ && emulator_->is_snes_initialized() &&
      emulator_->running()) {

    // CRITICAL: Verify audio stream resampling is still enabled
    // If disabled, samples play at wrong speed (1.5x due to 48000/32040 mismatch)
    if (auto* audio = emulator_->audio_backend()) {
      if (!audio->IsAudioStreamEnabled()) {
        LOG_ERROR("MusicPlayer", "AUDIO STREAM DISABLED during playback! Re-enabling...");
        audio->SetAudioStreamResampling(true, kNativeSampleRate, 2);
      }
    }

    // Simple frame pacing: check if enough time has passed for one frame
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - last_frame_time_).count();

    // Use emulator's frame timing (handles NTSC/PAL correctly)
    double frame_time = emulator_->wanted_frames();
    if (frame_time <= 0.0) {
      frame_time = 1.0 / 60.0988; // Fallback to NTSC
    }

    if (elapsed >= frame_time) {
      // DIAGNOSTIC: Check for timing anomalies
      static int speed_log_counter = 0;
      if (++speed_log_counter % 60 == 0) {
        double current_fps = 1.0 / elapsed;
        LOG_INFO("MusicPlayer", "Playback Speed: %.2f FPS (Target: %.2f), FrameTime: %.4fms, Wanted: %.4fms",
                 current_fps, 1.0/frame_time, elapsed*1000.0, frame_time*1000.0);
      }

      last_frame_time_ = now;

      // DIAGNOSTIC: Log which path we're taking
      static int frame_exec_count = 0;
      if (frame_exec_count < 5 || frame_exec_count % 300 == 0) {
        LOG_INFO("MusicPlayer", "Executing frame #%d: focus_mode=%d",
                 frame_exec_count, emulator_->is_audio_focus_mode());
      }
      frame_exec_count++;

      // Use simplified audio frame execution (RunAudioFrame processes exactly 1 frame)
      if (emulator_->is_audio_focus_mode()) {
        emulator_->RunAudioFrame();
      } else {
        emulator_->RunFrameOnly();
      }
    }

    // Debug: log APU cycle rate periodically
    static int debug_counter = 0;
    static uint64_t last_apu_cycles = 0;
    static auto last_log_time = std::chrono::steady_clock::now();

    if (++debug_counter % 60 == 0) {
      uint64_t apu_cycles = emulator_->snes().apu().GetCycles();
      auto now_log = std::chrono::steady_clock::now();
      auto log_elapsed = std::chrono::duration<double>(now_log - last_log_time).count();

      uint64_t cycle_delta = apu_cycles - last_apu_cycles;
      double cycles_per_sec = cycle_delta / log_elapsed;
      double rate_ratio = cycles_per_sec / 1024000.0;

      LOG_INFO("MusicPlayer", "APU: %llu cycles in %.2fs = %.0f/sec (%.2fx expected)",
               cycle_delta, log_elapsed, cycles_per_sec, rate_ratio);

      last_apu_cycles = apu_cycles;
      last_log_time = now_log;

      if (auto* audio = emulator_->audio_backend()) {
        auto status = audio->GetStatus();
        LOG_DEBUG("MusicPlayer", "Audio: playing=%d queued=%u bytes=%u",
                  status.is_playing, status.queued_frames, status.queued_bytes);
      }
    }
  }

  // Only poll game state when not in direct SPC mode
  if (!use_direct_spc_ && emulator_ && emulator_->is_snes_initialized()) {
    // Poll the game's current song ID (for game-based playback mode)
    // 0x7E012C is the RAM address for the current song ID in Zelda 3
    uint8_t current_song_id = emulator_->snes().Read(0x7E012C);

    // If the song ID changed externally (by the game), update our state
    // Note: Song IDs are 1-based in game, 0-based in editor
    if (current_song_id > 0 && (current_song_id - 1) != playing_song_index_) {
      playing_song_index_ = current_song_id - 1;

      // Reset timing if song changed
      playback_start_time_ = std::chrono::steady_clock::now();
      playback_start_tick_ = 0;
      playback_segment_index_ = 0;

      // Update tempo for the new song
      if (music_bank_) {
        auto* song = music_bank_->GetSong(playing_song_index_);
        if (song) {
          uint8_t tempo = GetSongTempo(*song);
          ticks_per_second_ = CalculateTicksPerSecond(tempo);
        }
      }

      // Update mode if not already playing
      if (mode_ == PlaybackMode::Stopped) {
        TransitionTo(PlaybackMode::Playing);
      }
    }
  }
}

bool MusicPlayer::IsAudioReady() const {
  // We are ready if we have a ROM. Backend is set up lazily in EnsureAudioReady().
  return rom_ != nullptr;
}

bool MusicPlayer::EnsureAudioReady() {
  if (!rom_) {
    LOG_WARN("MusicPlayer", "EnsureAudioReady: No ROM loaded");
    return false;
  }

  if (!emulator_) {
    LOG_ERROR("MusicPlayer", "EnsureAudioReady: No emulator set");
    return false;
  }

  if (!emulator_->is_snes_initialized()) {
    LOG_INFO("MusicPlayer", "Initializing SNES for audio playback...");
    if (!emulator_->EnsureInitialized(rom_)) {
      LOG_ERROR("MusicPlayer", "Failed to initialize emulator");
      return false;
    }
  }

  // CRITICAL: Enable SDL audio stream mode on the emulator FIRST.
  emulator_->set_use_sdl_audio_stream(true);
  LOG_INFO("MusicPlayer", "Set use_sdl_audio_stream=true, wanted_samples=%d",
           emulator_->wanted_samples());

  // Enable audio stream resampling for proper 32kHz -> 48kHz conversion
  if (auto* audio = emulator_->audio_backend()) {
    auto config = audio->GetConfig();
    LOG_INFO("MusicPlayer", "Audio backend: %s, config=%dHz/%dch, initialized=%d",
             audio->GetBackendName().c_str(), config.sample_rate, config.channels,
             audio->IsInitialized());

    if (audio->SupportsAudioStream()) {
      LOG_INFO("MusicPlayer", "Calling SetAudioStreamResampling(%d Hz -> %d Hz)",
               kNativeSampleRate, config.sample_rate);
      audio->SetAudioStreamResampling(true, kNativeSampleRate, 2);
      // Prevent RunAudioFrame() from overriding our configuration
      emulator_->mark_audio_stream_configured();
    }
  } else {
    LOG_ERROR("MusicPlayer", "No audio backend available!");
    return false;
  }

  if (!spc_initialized_) {
    InitializeDirectSpc();
    if (!spc_initialized_) {
      LOG_ERROR("MusicPlayer", "Failed to initialize SPC");
      return false;
    }
  }

  emulator_->set_interpolation_type(interpolation_type_);

  return true;
}

bool MusicPlayer::EnsurePreviewReady() {
  if (!EnsureAudioReady()) return false;

  if (preview_initialized_) return true;

  InitializePreviewMode();
  return preview_initialized_;
}

void MusicPlayer::InitializeDirectSpc() {
  if (!emulator_ || !rom_) return;
  // Force re-initialization if requested (spc_initialized_ is false)

  auto& apu = emulator_->snes().apu();
  LOG_INFO("MusicPlayer", "Initializing direct SPC playback");

  preview_initialized_ = false;

  // Reset APU
  apu.Reset();

  // Upload Driver (Bank 0)
  UploadSoundBankFromRom(GetBankRomOffset(0));
  // PatchDriver removed - fixing root cause in APU emulation instead

  // 4. Start Driver
  apu.BootstrapDirect(kDriverEntryPoint);

  // Initialize song pointers
  // UploadSongToAram(song_pointers, 0xD000);

  // 5. Run init cycles
  for (int i = 0; i < kSpcResetCycles; i++) {
    apu.Cycle();
  }

  spc_initialized_ = true;
  current_spc_bank_ = 0xFF;
}

void MusicPlayer::InitializePreviewMode() {
  if (!emulator_ || !rom_) return;
  if (preview_initialized_) return;

  auto& apu = emulator_->snes().apu();
  LOG_INFO("MusicPlayer", "Initializing preview mode");

  apu.Reset();
  UploadSoundBankFromRom(GetBankRomOffset(0));

  apu.WriteToDsp(kDspDir, 0x3C);
  apu.WriteToDsp(kDspKeyOn, 0x00);
  apu.WriteToDsp(kDspKeyOff, 0x00);
  apu.WriteToDsp(kDspMainVolL, 0x7F);
  apu.WriteToDsp(kDspMainVolR, 0x7F);
  apu.WriteToDsp(kDspEchoVolL, 0x00);
  apu.WriteToDsp(kDspEchoVolR, 0x00);
  apu.WriteToDsp(kDspFlg, 0x20);

  for (int i = 0; i < 1000; i++) {
    apu.Cycle();
  }

  preview_initialized_ = true;
}

void MusicPlayer::PlaySong(int song_index) {
  if (!rom_) {
    LOG_WARN("MusicPlayer", "No ROM loaded - cannot play song");
    return;
  }

  // Stop any existing playback (check mode, not legacy flag)
  if (mode_ != PlaybackMode::Stopped) {
    Stop();
  }

  if (use_direct_spc_) {
    PlaySongDirect(song_index + 1); // 1-based ID for game
    return;
  }

  // Request exclusive audio control BEFORE initializing to prevent audio mixing
  if (audio_exclusivity_callback_) {
    LOG_INFO("MusicPlayer", "Pre-requesting exclusive audio control for game-based playback");
    audio_exclusivity_callback_(true);
  }

  // Game-based playback logic
  if (!EnsureAudioReady()) return;

  emulator_->set_interpolation_type(interpolation_type_);

  if (!emulator_->running()) {
    emulator_->set_running(true);
  }

  if (auto* audio = emulator_->audio_backend()) {
    // Prime the buffer with silence to prevent immediate underrun
    // Queue ~6 frames (100ms) of silence
    constexpr int kPrimeFrames = 6;
    constexpr int kPrimeSamples = 533 * kPrimeFrames;
    std::vector<int16_t> silence(kPrimeSamples * 2, 0); // Stereo
    
    constexpr int kNativeSampleRate = 32040;
    audio->QueueSamplesNative(silence.data(), kPrimeSamples, 2, kNativeSampleRate);

    if (!audio->GetStatus().is_playing) {
      audio->Play();
    }
  }

  // Write song ID to game RAM (game-based playback mode)
  emulator_->snes().Write(0x7E012C, static_cast<uint8_t>(song_index + 1));

  // Update playback state
  playing_song_index_ = song_index;
  playback_start_time_ = std::chrono::steady_clock::now();
  playback_start_tick_ = 0;
  playback_segment_index_ = 0;

  // Calculate timing
  auto* song = music_bank_->GetSong(song_index);
  uint8_t tempo = song ? GetSongTempo(*song) : 150;
  ticks_per_second_ = CalculateTicksPerSecond(tempo);

  // Initialize frame timing for Update() loop - CRITICAL for correct playback speed
  last_frame_time_ = std::chrono::steady_clock::now();

  TransitionTo(PlaybackMode::Playing);
}

void MusicPlayer::PlaySongDirect(int song_id) {
  if (!rom_) return;

  // IMPORTANT: Initialize audio backend FIRST, then request exclusivity
  // If we pause main emulator before our backend is ready, we get silence on first play
  if (preview_initialized_) {
    InitializeDirectSpc();
  }

  if (!EnsureAudioReady()) return;

  // NOW request exclusive audio control - our backend is ready to take over
  if (audio_exclusivity_callback_) {
    LOG_INFO("MusicPlayer", "Requesting exclusive audio control (backend ready)");
    audio_exclusivity_callback_(true);
  }

  int song_index = song_id - 1;
  const zelda3::music::MusicSong* song = nullptr;

  if (music_bank_ && song_index >= 0 && song_index < music_bank_->GetSongCount()) {
    song = music_bank_->GetSong(song_index);
  }

  if (!song) return;

  if (song->modified) {
    PreviewCustomSong(song_index);
    return;
  }

  uint8_t song_bank = song->bank;
  bool is_expanded = (song_bank == 3 || song_bank == 4);

  LOG_INFO("MusicPlayer", "Playing song %d (%s) from song_bank=%d",
           song_id, song->name.c_str(), song_bank);

  auto& apu = emulator_->snes().apu();

  if (current_spc_bank_ != song_bank) {
    uint8_t rom_bank = song_bank + 1;
    if (is_expanded && !music_bank_->HasExpandedMusicPatch()) {
      rom_bank = 1;
      song_bank = 0;
    }
    UploadSoundBankFromRom(GetBankRomOffset(rom_bank));
    current_spc_bank_ = song_bank;
  }

  // Ensure audio backend is ready
  emulator_->set_interpolation_type(interpolation_type_);
  if (auto* audio = emulator_->audio_backend()) {
    // audio_ready_ is removed
  }

  // Calculate SPC song index
  uint8_t spc_song_index;
  if (is_expanded) {
    int vanilla_count = 34;
    int expanded_index = (song_id - 1) - vanilla_count;
    spc_song_index = static_cast<uint8_t>(expanded_index + 1);
  } else {
    spc_song_index = static_cast<uint8_t>(song_id);
  }

  // Trigger song playback via APU ports
  static uint8_t trigger_byte = 0x00;
  trigger_byte ^= 0x01;

  apu.in_ports_[0] = spc_song_index;
  apu.in_ports_[1] = trigger_byte;

  // Run APU cycles to let the driver start the song
  // This also generates initial audio samples
  for (int i = 0; i < kSpcInitCycles; i++) {
    apu.Cycle();
  }

  // Clear any stale audio from previous playback before priming
  if (auto* audio = emulator_->audio_backend()) {
    audio->Clear();
  }

  // Reset DSP sample buffer for clean start - prevents stale samples from
  // previous playback causing timing/position issues on first play
  auto& dsp = emulator_->snes().apu().dsp();
  dsp.ResetSampleBuffer();

  // Run one full frame worth of audio generation to fill the buffer
  // Note: RunAudioFrame() handles NewFrame() internally at vblank
  emulator_->snes().RunAudioFrame();

  // Now reset timing and start playback with buffer already primed
  emulator_->ResetFrameTiming();

  // Prime the audio queue with initial samples
  constexpr int kNativeSampleRate = 32040;
  constexpr int kInitialSamples = 533;  // ~1 frame worth
  static int16_t prime_buffer[2048];
  std::memset(prime_buffer, 0, sizeof(prime_buffer));
  emulator_->snes().SetSamples(prime_buffer, kInitialSamples);
  if (auto* audio = emulator_->audio_backend()) {
    bool queued = audio->QueueSamplesNative(prime_buffer, kInitialSamples, 2, kNativeSampleRate);
    LOG_INFO("MusicPlayer", "Initial samples queued: %s", queued ? "YES" : "NO (RESAMPLING FAILED!)");
  }

  // Start audio playback
  if (auto* audio = emulator_->audio_backend()) {
    // Prime the buffer with silence to prevent immediate underrun
    // Queue ~6 frames (100ms) of silence
    constexpr int kPrimeFrames = 6;
    constexpr int kPrimeSamples = 533 * kPrimeFrames;
    std::vector<int16_t> silence(kPrimeSamples * 2, 0); // Stereo

    // Use the native rate (32040) so it gets resampled correctly if needed
    constexpr int kNativeSampleRate2 = 32040;
    bool silenceQueued = audio->QueueSamplesNative(silence.data(), kPrimeSamples, 2, kNativeSampleRate2);
    LOG_INFO("MusicPlayer", "Silence buffer queued: %s", silenceQueued ? "YES" : "NO (RESAMPLING FAILED!)");

    auto status = audio->GetStatus();
    LOG_INFO("MusicPlayer", "Audio status before Play(): playing=%d, queued_frames=%u",
             status.is_playing, status.queued_frames);

    // Always call Play() to ensure audio device is ready
    // SDL's Play() is idempotent - safe to call even if already playing
    audio->Play();

    status = audio->GetStatus();
    LOG_INFO("MusicPlayer", "Audio status after Play(): playing=%d, queued_frames=%u",
             status.is_playing, status.queued_frames);
  }

  // Enable audio-focused mode for efficient playback
  emulator_->set_audio_focus_mode(true);
  emulator_->set_running(true);

  // Update playback state
  playing_song_index_ = song_id - 1;
  playback_start_time_ = std::chrono::steady_clock::now();
  playback_start_tick_ = 0;
  playback_segment_index_ = 0;

  // Calculate timing for this song
  uint8_t tempo = GetSongTempo(*song);
  ticks_per_second_ = CalculateTicksPerSecond(tempo);

  // Initialize frame timing for Update() loop - CRITICAL for correct playback speed
  last_frame_time_ = std::chrono::steady_clock::now();

  TransitionTo(PlaybackMode::Playing);
  LOG_INFO("MusicPlayer", "Started playing song %d at %.1f ticks/sec",
           playing_song_index_, ticks_per_second_);
}

void MusicPlayer::Pause() {
  if (!emulator_) return;
  if (mode_ != PlaybackMode::Playing && mode_ != PlaybackMode::Previewing) return;

  // Save current position before pausing
  playback_start_tick_ = GetCurrentPlaybackTick();

  // Pause emulator and audio
  emulator_->set_running(false);
  if (auto* audio = emulator_->audio_backend()) {
    audio->Pause();
  }

  TransitionTo(PlaybackMode::Paused);
  LOG_DEBUG("MusicPlayer", "Paused at tick %u", playback_start_tick_);
}

void MusicPlayer::Resume() {
  if (!emulator_) return;
  if (mode_ != PlaybackMode::Paused) return;

  // Reset frame timing to prevent accumulated time from causing fast-forward
  emulator_->ResetFrameTiming();
  emulator_->set_running(true);

  if (auto* audio = emulator_->audio_backend()) {
    audio->Clear(); // Clear buffer to prevent stale audio
    audio->Play();
  }

  // Start counting from where we paused
  playback_start_time_ = std::chrono::steady_clock::now();

  // Initialize frame timing for Update() loop - CRITICAL for correct playback speed
  last_frame_time_ = std::chrono::steady_clock::now();

  TransitionTo(PlaybackMode::Playing);
  LOG_DEBUG("MusicPlayer", "Resumed from tick %u", playback_start_tick_);
}

void MusicPlayer::Stop() {
  if (!emulator_) return;
  if (mode_ == PlaybackMode::Stopped) return;

  // Send stop command to SPC700
  auto& apu = emulator_->snes().apu();
  apu.in_ports_[0] = 0x00;
  apu.in_ports_[1] = 0xFF;  // Stop command

  // Run APU cycles to process the stop command
  for (int i = 0; i < kSpcStopCycles; i++) {
    apu.Cycle();
  }

  // Stop emulator and audio
  emulator_->set_running(false);
  emulator_->set_audio_focus_mode(false);

  if (auto* audio = emulator_->audio_backend()) {
    audio->Stop();
    audio->Clear();  // Clear stale audio to prevent glitches on restart
  }

  // Reset state but keep playing_song_index_ for TogglePlayPause() replay
  // playing_song_index_ is intentionally NOT reset to -1
  playback_start_tick_ = 0;
  playback_segment_index_ = 0;
  ticks_per_second_ = 0.0f;

  TransitionTo(PlaybackMode::Stopped);
  LOG_DEBUG("MusicPlayer", "Stopped playback");
}

void MusicPlayer::SetVolume(float volume) {
  if (emulator_ && emulator_->audio_backend()) {
    emulator_->audio_backend()->SetVolume(std::clamp(volume, 0.0f, 1.0f));
  }
}

void MusicPlayer::SetPlaybackSpeed(float /*speed*/) {
  // Varispeed removed - always plays at 1.0x speed for correct audio timing
  // The playback_speed_ member no longer exists
}

void MusicPlayer::SetInterpolationType(int type) {
  interpolation_type_ = type;
  if (emulator_ && emulator_->is_snes_initialized()) {
    emulator_->set_interpolation_type(type);
  }
}

void MusicPlayer::SetDirectSpcMode(bool enabled) {
  use_direct_spc_ = enabled;
}

void MusicPlayer::UploadSoundBankFromRom(uint32_t rom_offset) {
  if (!emulator_ || !rom_) return;

  auto& apu = emulator_->snes().apu();
  const uint8_t* rom_data = rom_->data();
  const size_t rom_size = rom_->size();

  LOG_INFO("MusicPlayer", "Uploading sound bank from ROM offset 0x%X", rom_offset);

  int block_count = 0;
  while (rom_offset + 4 < rom_size) {
    uint16_t block_size = rom_data[rom_offset] | (rom_data[rom_offset + 1] << 8);
    uint16_t aram_addr = rom_data[rom_offset + 2] | (rom_data[rom_offset + 3] << 8);

    if (block_size == 0 || block_size > 0x10000) {
      break;
    }

    if (rom_offset + 4 + block_size > rom_size) {
      LOG_WARN("MusicPlayer", "Block at 0x%X extends past ROM end", rom_offset);
      break;
    }

    apu.WriteDma(aram_addr, &rom_data[rom_offset + 4], block_size);

    rom_offset += 4 + block_size;
    block_count++;
  }
}

void MusicPlayer::UploadSongToAram(const std::vector<uint8_t>& data, uint16_t aram_address) {
  if (!emulator_) return;
  auto& apu = emulator_->snes().apu();
  for (size_t i = 0; i < data.size(); ++i) {
    apu.ram[aram_address + i] = data[i];
  }
}

uint32_t MusicPlayer::GetBankRomOffset(uint8_t bank) const {
  if (bank < 6) {
    return kSoundBankOffsets[bank];
  }
  return kSoundBankOffsets[0];
}

int MusicPlayer::GetSongIndexInBank(int song_id, uint8_t bank) const {
  switch (bank) {
    case 0: return song_id - 1;
    case 1: return song_id - 12;
    case 2: return song_id - 32;
    default: return 0;
  }
}

uint8_t MusicPlayer::GetSongTempo(const zelda3::music::MusicSong& song) const {
  constexpr uint8_t kDefaultTempo = 150;
  if (song.segments.empty()) return kDefaultTempo;

  const auto& segment = song.segments[0];
  for (const auto& track : segment.tracks) {
    for (const auto& event : track.events) {
      if (event.type == zelda3::music::TrackEvent::Type::Command &&
          event.command.opcode == kOpcodeTempo) {
        return event.command.params[0];
      }
    }
  }
  return kDefaultTempo;
}

float MusicPlayer::CalculateTicksPerSecond(uint8_t tempo) const {
  // The SNES SPC700 driver uses a timer (usually Timer 0) running at 8000Hz.
  // The timer has a divider (usually 16), resulting in a 500Hz base tick.
  // The driver accumulates the tempo value every 500Hz tick.
  // When the accumulator overflows, a music tick is generated.
  // Formula: Rate = Base_Freq * (Tempo / 256)
  // Base_Freq = 8000 / Divider (16) = 500Hz
  
  return 500.0f * (static_cast<float>(tempo) / 256.0f);
}

uint32_t MusicPlayer::GetCurrentPlaybackTick() const {
  // Only count ticks when actively playing (not stopped or paused)
  bool is_active = (mode_ == PlaybackMode::Playing || mode_ == PlaybackMode::Previewing);
  if (!is_active) return playback_start_tick_;

  auto now = std::chrono::steady_clock::now();
  float elapsed_seconds = std::chrono::duration<float>(now - playback_start_time_).count();

  return playback_start_tick_ + static_cast<uint32_t>(elapsed_seconds * ticks_per_second_);
}

// Preview methods (ported from MusicEditor)
void MusicPlayer::PreviewNote(const zelda3::music::MusicSong& song,
                              const zelda3::music::TrackEvent& event,
                              int segment_index, int channel_index) {
  if (event.type != zelda3::music::TrackEvent::Type::Note || !event.note.IsNote()) {
    return;
  }

  if (!EnsureAudioReady()) return;

  auto& apu = emulator_->snes().apu();

  // Resolve instrument
  const zelda3::music::MusicSegment* segment = nullptr;
  if (segment_index >= 0 && segment_index < static_cast<int>(song.segments.size())) {
    segment = &song.segments[segment_index];
  }

  // Helper to resolve instrument (duplicated logic for now, could be shared)
  int instrument_index = -1;
  if (segment && channel_index >= 0 && channel_index < 8) {
    const auto& track = segment->tracks[channel_index];
    for (const auto& evt : track.events) {
      if (evt.tick > event.tick) break;
      if (evt.type == zelda3::music::TrackEvent::Type::Command && evt.command.opcode == 0xE0) {
        instrument_index = evt.command.params[0];
      }
    }
  }

  const auto* instrument = music_bank_->GetInstrument(instrument_index);

  int ch_base = channel_index * 0x10;
  int inst_idx = instrument ? instrument->sample_index : 0;

  apu.WriteToDsp(ch_base + kDspSrcn, inst_idx);

  uint16_t pitch = zelda3::music::LookupNSpcPitch(event.note.pitch);
  apu.WriteToDsp(ch_base + kDspPitchLow, pitch & 0xFF);
  apu.WriteToDsp(ch_base + kDspPitchHigh, (pitch >> 8) & 0x3F);

  apu.WriteToDsp(ch_base + kDspVolL, 0x7F);
  apu.WriteToDsp(ch_base + kDspVolR, 0x7F);

  if (instrument) {
    apu.WriteToDsp(ch_base + kDspAdsr1, instrument->GetADByte());
    apu.WriteToDsp(ch_base + kDspAdsr2, instrument->GetSRByte());
  } else {
    apu.WriteToDsp(ch_base + kDspAdsr1, 0xFF);
    apu.WriteToDsp(ch_base + kDspAdsr2, 0xE0);
  }

  apu.WriteToDsp(kDspKeyOn, 1 << channel_index);

  for (int i = 0; i < kSpcPreviewCycles; i++) apu.Cycle();

  PrepareAudioPlayback();
  TransitionTo(PlaybackMode::Previewing);
}

ChannelState MusicPlayer::GetChannelState(int channel_index) const {
  ChannelState state;
  if (!emulator_ || !emulator_->is_snes_initialized() || channel_index < 0 || channel_index >= 8) {
    return state; // Default initialized
  }
  const auto& dsp = emulator_->snes().apu().dsp();
  const auto& ch = dsp.GetChannel(channel_index);
  state.key_on = ch.keyOn;
  state.sample_index = ch.srcn;
  state.pitch = ch.pitch;
  state.volume_l = static_cast<uint8_t>(std::abs(ch.volumeL));
  state.volume_r = static_cast<uint8_t>(std::abs(ch.volumeR));
  state.gain = ch.gain;
  state.adsr_state = ch.adsrState;
  return state;
}

std::array<ChannelState, 8> MusicPlayer::GetChannelStates() const {
  std::array<ChannelState, 8> states;
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return states; // Default initialized
  }

  const auto& dsp = emulator_->snes().apu().dsp();
  for (int i = 0; i < 8; ++i) {
    const auto& ch = dsp.GetChannel(i);
    states[i].key_on = ch.keyOn;
    states[i].sample_index = ch.srcn;
    states[i].pitch = ch.pitch;
    states[i].volume_l = static_cast<uint8_t>(std::abs(ch.volumeL)); // Use abs for visualization
    states[i].volume_r = static_cast<uint8_t>(std::abs(ch.volumeR));
    states[i].gain = ch.gain;
    states[i].adsr_state = ch.adsrState;
  }
  return states;
}

void MusicPlayer::PreviewSegment(const zelda3::music::MusicSong& song, int segment_index) {
  if (!EnsureAudioReady()) return;
  if (segment_index < 0 || segment_index >= static_cast<int>(song.segments.size())) return;

  zelda3::music::MusicSong temp_song;
  temp_song.name = "Preview Segment";
  temp_song.bank = song.bank;
  temp_song.segments.push_back(song.segments[segment_index]);
  temp_song.loop_point = -1;

  uint16_t base_aram = zelda3::music::kSongTableAram;
  auto result = zelda3::music::SpcSerializer::SerializeSong(temp_song, base_aram);
  if (!result.ok()) {
    LOG_ERROR("MusicPlayer", "Failed to serialize segment: %s", result.status().message().data());
    return;
  }

  UploadSongToAram(result->data, result->base_address);

  UploadSongToAram(result->data, result->base_address);

  auto& apu = emulator_->snes().apu();
  static uint8_t trigger = 0x00;
  trigger ^= 0x01;

  apu.in_ports_[0] = 1;
  apu.in_ports_[1] = trigger;

  PrepareAudioPlayback();

  // Calculate segment start tick for timeline positioning
  uint32_t segment_start_tick = 0;
  for (int i = 0; i < segment_index; ++i) {
    segment_start_tick += song.segments[i].GetDuration();
  }

  playback_start_time_ = std::chrono::steady_clock::now();
  playback_start_tick_ = segment_start_tick;
  playback_segment_index_ = segment_index;

  uint8_t tempo = GetSongTempo(song);
  ticks_per_second_ = CalculateTicksPerSecond(tempo);

  TransitionTo(PlaybackMode::Previewing);
  LOG_DEBUG("MusicPlayer", "Previewing segment %d at tick %u", segment_index, segment_start_tick);
}

void MusicPlayer::PreviewInstrument(int instrument_index) {
  if (!EnsurePreviewReady()) return;

  auto* instrument = music_bank_->GetInstrument(instrument_index);
  if (!instrument) return;

  auto& apu = emulator_->snes().apu();
  int ch = 0;
  int ch_base = ch * 0x10;

  // Clear any stale audio before preview
  if (auto* audio = emulator_->audio_backend()) {
    audio->Clear();
  }

  apu.WriteToDsp(kDspKeyOff, 1 << ch);
  for(int i=0; i<500; ++i) apu.Cycle();  // More cycles for DSP to stabilize

  apu.WriteToDsp(ch_base + kDspSrcn, instrument->sample_index);
  apu.WriteToDsp(ch_base + kDspAdsr1, instrument->GetADByte());
  apu.WriteToDsp(ch_base + kDspAdsr2, instrument->GetSRByte());
        apu.WriteToDsp(ch_base + kDspGain, instrument->gain);
  
        uint16_t pitch = zelda3::music::LookupNSpcPitch(0x80 + 36); // C4
        pitch = (static_cast<uint32_t>(pitch) * instrument->pitch_mult) >> 12;
  
        apu.WriteToDsp(ch_base + kDspPitchLow, pitch & 0xFF);
        apu.WriteToDsp(ch_base + kDspPitchHigh, (pitch >> 8) & 0x3F);
  
  apu.WriteToDsp(ch_base + kDspVolL, 0x7F);
  apu.WriteToDsp(ch_base + kDspVolR, 0x7F);

  apu.WriteToDsp(kDspKeyOn, 1 << ch);

  PrepareAudioPlayback();
  TransitionTo(PlaybackMode::Previewing);
}

void MusicPlayer::PreviewSample(int sample_index) {
  if (!EnsurePreviewReady()) return;

  auto* sample = music_bank_->GetSample(sample_index);
  if (!sample) return;

  uint16_t temp_addr = 0x8000;
  UploadSongToAram(sample->brr_data, temp_addr);

  uint16_t loop_addr = temp_addr + sample->loop_point;
  std::vector<uint8_t> dir = {
    static_cast<uint8_t>(temp_addr & 0xFF),
    static_cast<uint8_t>(temp_addr >> 8),
    static_cast<uint8_t>(loop_addr & 0xFF),
    static_cast<uint8_t>(loop_addr >> 8)
  };
  UploadSongToAram(dir, 0x3C00);

  auto& apu = emulator_->snes().apu();
  int ch = 0;
  int ch_base = ch * 0x10;

  // Clear any stale audio before preview
  if (auto* audio = emulator_->audio_backend()) {
    audio->Clear();
  }

  apu.WriteToDsp(kDspKeyOff, 1 << ch);
  for(int i=0; i<500; ++i) apu.Cycle();  // More cycles for DSP to stabilize

  apu.WriteToDsp(ch_base + kDspSrcn, 0x00); // Sample 0
  apu.WriteToDsp(ch_base + kDspAdsr1, 0xFF);
  apu.WriteToDsp(ch_base + kDspAdsr2, 0xE0);
  apu.WriteToDsp(ch_base + kDspGain, 0x7F);

  uint16_t pitch = 0x1000;
  apu.WriteToDsp(ch_base + kDspPitchLow, pitch & 0xFF);
  apu.WriteToDsp(ch_base + kDspPitchHigh, (pitch >> 8) & 0x3F);

  apu.WriteToDsp(ch_base + kDspVolL, 0x7F);
  apu.WriteToDsp(ch_base + kDspVolR, 0x7F);

  apu.WriteToDsp(kDspKeyOn, 1 << ch);

  PrepareAudioPlayback();
  TransitionTo(PlaybackMode::Previewing);
}

void MusicPlayer::PreviewCustomSong(int song_index) {
  if (!EnsureAudioReady()) return;

  auto* song = music_bank_->GetSong(song_index);
  if (!song) return;

  LOG_INFO("MusicPlayer", "Previewing custom song: %s", song->name.c_str());

  // Serialize the modified song from memory
  uint16_t base_aram = zelda3::music::kSongTableAram;
  auto result = zelda3::music::SpcSerializer::SerializeSong(*song, base_aram);
  if (!result.ok()) {
    LOG_ERROR("MusicPlayer", "Failed to serialize song: %s", result.status().message().data());
    return;
  }

  // Upload serialized song data to APU RAM
  UploadSongToAram(result->data, result->base_address);

  // Upload serialized song data to APU RAM
  UploadSongToAram(result->data, result->base_address);

  auto& apu = emulator_->snes().apu();

  // Trigger song 1 (our uploaded song is at the beginning of the table)
  apu.in_ports_[0] = 1;
  apu.in_ports_[1] = 0x00;

  // Run APU cycles to start playback
  for (int i = 0; i < kSpcInitCycles; i++) apu.Cycle();

  PrepareAudioPlayback();

  // Update state
  playing_song_index_ = song_index;
  playback_start_time_ = std::chrono::steady_clock::now();
  playback_start_tick_ = 0;
  playback_segment_index_ = 0;

  uint8_t tempo = GetSongTempo(*song);
  ticks_per_second_ = CalculateTicksPerSecond(tempo);

  TransitionTo(PlaybackMode::Previewing);
}

void MusicPlayer::SeekToSegment(int segment_index) {
  auto* song = music_bank_->GetSong(playing_song_index_);
  if (!song || segment_index < 0 ||
      segment_index >= static_cast<int>(song->segments.size())) {
    return;
  }

  // Calculate tick offset for this segment
  uint32_t tick_offset = 0;
  for (int i = 0; i < segment_index; ++i) {
    tick_offset += song->segments[i].GetDuration();
  }

  playback_start_time_ = std::chrono::steady_clock::now();
  playback_start_tick_ = tick_offset;
  playback_segment_index_ = segment_index;

  // Update tempo from segment (use first track's tempo if available)
  const auto& segment = song->segments[segment_index];
  if (!segment.tracks.empty()) {
    for (const auto& event : segment.tracks[0].events) {
      if (event.type == zelda3::music::TrackEvent::Type::Command &&
          event.command.opcode == kOpcodeTempo) {  // Tempo command
        ticks_per_second_ = CalculateTicksPerSecond(event.command.params[0]);
        break;
      }
    }
  }
}

const zelda3::music::MusicInstrument* MusicPlayer::ResolveInstrumentForEvent(
    const zelda3::music::MusicSegment& segment, int channel_index,
    uint16_t tick) const {
  if (channel_index < 0 || channel_index >= 8) return nullptr;

  int instrument_index = -1;
  const auto& track = segment.tracks[channel_index];

  for (const auto& evt : track.events) {
    if (evt.tick > tick) break;
    if (evt.type == zelda3::music::TrackEvent::Type::Command &&
        evt.command.opcode == 0xE0) { // SetInstrument
      instrument_index = evt.command.params[0];
    }
  }

  if (instrument_index == -1) return nullptr;
  return music_bank_->GetInstrument(instrument_index);
}

// === Debug Diagnostics ===

DspDebugStatus MusicPlayer::GetDspStatus() const {
  DspDebugStatus status;
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return status;
  }

  const auto& dsp = emulator_->snes().apu().dsp();
  status.sample_offset = dsp.GetSampleOffset();
  status.frame_boundary = dsp.GetFrameBoundary();
  status.master_vol_l = dsp.GetMasterVolumeL();
  status.master_vol_r = dsp.GetMasterVolumeR();
  status.mute = dsp.IsMuted();
  status.reset = dsp.IsReset();
  status.echo_enabled = dsp.IsEchoEnabled();
  status.echo_delay = dsp.GetEchoDelay();
  return status;
}

ApuDebugStatus MusicPlayer::GetApuStatus() const {
  ApuDebugStatus status;
  if (!emulator_ || !emulator_->is_snes_initialized()) {
    return status;
  }

  const auto& apu = emulator_->snes().apu();
  status.cycles = apu.GetCycles();

  // Timer 0
  const auto& t0 = apu.GetTimer(0);
  status.timer0_enabled = t0.enabled;
  status.timer0_counter = t0.counter;
  status.timer0_target = t0.target;

  // Timer 1
  const auto& t1 = apu.GetTimer(1);
  status.timer1_enabled = t1.enabled;
  status.timer1_counter = t1.counter;
  status.timer1_target = t1.target;

  // Timer 2
  const auto& t2 = apu.GetTimer(2);
  status.timer2_enabled = t2.enabled;
  status.timer2_counter = t2.counter;
  status.timer2_target = t2.target;

  // Port state
  status.port0_in = apu.in_ports_[0];
  status.port1_in = apu.in_ports_[1];
  status.port0_out = apu.out_ports_[0];
  status.port1_out = apu.out_ports_[1];

  return status;
}

AudioQueueStatus MusicPlayer::GetAudioQueueStatus() const {
  AudioQueueStatus status;
  if (!emulator_) {
    return status;
  }

  if (auto* audio = emulator_->audio_backend()) {
    auto backend_status = audio->GetStatus();
    status.is_playing = backend_status.is_playing;
    status.queued_frames = backend_status.queued_frames;
    status.queued_bytes = backend_status.queued_bytes;
    status.has_underrun = backend_status.has_underrun;

    auto config = audio->GetConfig();
    status.sample_rate = config.sample_rate;
    status.backend_name = audio->GetBackendName();
  }

  return status;
}

// === Debug Actions ===

void MusicPlayer::ClearAudioQueue() {
  if (!emulator_) return;

  if (auto* audio = emulator_->audio_backend()) {
    audio->Clear();
    LOG_INFO("MusicPlayer", "Audio queue cleared");
  }
}

void MusicPlayer::ResetDspBuffer() {
  if (!emulator_ || !emulator_->is_snes_initialized()) return;

  auto& dsp = emulator_->snes().apu().dsp();
  dsp.ResetSampleBuffer();
  LOG_INFO("MusicPlayer", "DSP buffer reset");
}

void MusicPlayer::ForceNewFrame() {
  if (!emulator_ || !emulator_->is_snes_initialized()) return;

  auto& dsp = emulator_->snes().apu().dsp();
  dsp.NewFrame();
  LOG_INFO("MusicPlayer", "Forced DSP NewFrame()");
}

void MusicPlayer::ReinitAudio() {
  if (!emulator_) return;

  // Stop current playback
  Stop();

  // Reset SPC initialization state to force reinit on next play
  spc_initialized_ = false;
  preview_initialized_ = false;
  // audio_ready_ removed
  current_spc_bank_ = 0xFF;

  LOG_INFO("MusicPlayer", "Audio system marked for reinitialization");
}

} // namespace music
} // namespace editor
} // namespace yaze
