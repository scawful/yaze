#ifndef YAZE_APP_EDITOR_MUSIC_MUSIC_PLAYER_H
#define YAZE_APP_EDITOR_MUSIC_MUSIC_PLAYER_H

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/music/music_constants.h"
#include "app/emu/audio/audio_backend.h"
#include "app/emu/emulator.h"
#include "zelda3/music/music_bank.h"
#include "zelda3/music/song_data.h"

namespace yaze {

class Rom;

namespace editor {
namespace music {

/**
 * @brief Represents the current playback state of the music player.
 */
struct PlaybackState {
  bool is_playing = false;
  bool is_paused = false;
  int playing_song_index = -1;
  float playback_speed = 1.0f;
  uint32_t current_tick = 0;
  int current_segment_index = 0;
  float ticks_per_second = 0.0f;
};

/**
 * @brief Represents the state of a single DSP channel for visualization.
 */
struct ChannelState {
  bool key_on = false;
  int sample_index = 0;
  uint16_t pitch = 0;
  uint8_t volume_l = 0;
  uint8_t volume_r = 0;
  uint8_t gain = 0;
  uint8_t adsr_state = 0;  // 0: attack, 1: decay, 2: sustain, 3: release
};

/**
 * @brief DSP buffer diagnostic status for debug UI.
 */
struct DspDebugStatus {
  uint16_t sample_offset = 0;      // Current write position in ring buffer (0-2047)
  uint32_t frame_boundary = 0;     // Position of last frame boundary
  int8_t master_vol_l = 0;         // Master volume left
  int8_t master_vol_r = 0;         // Master volume right
  bool mute = false;               // DSP mute flag
  bool reset = false;              // DSP reset flag
  bool echo_enabled = false;       // Echo writes enabled
  uint16_t echo_delay = 0;         // Echo delay setting
};

/**
 * @brief APU timing diagnostic status for debug UI.
 */
struct ApuDebugStatus {
  uint64_t cycles = 0;             // Total APU cycles executed
  // Timer 0
  bool timer0_enabled = false;
  uint8_t timer0_counter = 0;
  uint8_t timer0_target = 0;
  // Timer 1
  bool timer1_enabled = false;
  uint8_t timer1_counter = 0;
  uint8_t timer1_target = 0;
  // Timer 2
  bool timer2_enabled = false;
  uint8_t timer2_counter = 0;
  uint8_t timer2_target = 0;
  // Port state
  uint8_t port0_in = 0;
  uint8_t port1_in = 0;
  uint8_t port0_out = 0;
  uint8_t port1_out = 0;
};

/**
 * @brief Audio queue diagnostic status for debug UI.
 */
struct AudioQueueStatus {
  bool is_playing = false;
  uint32_t queued_frames = 0;
  uint32_t queued_bytes = 0;
  bool has_underrun = false;
  int sample_rate = 0;
  std::string backend_name;
};

/**
 * @brief Playback mode for the music player.
 */
enum class PlaybackMode {
  Stopped,     // No playback
  Playing,     // Active playback
  Paused,      // Playback paused
  Previewing   // Single note/segment preview
};

/**
 * @class MusicPlayer
 * @brief Handles audio playback for the music editor using the SNES APU emulator.
 *
 * The MusicPlayer manages playback of songs from ROM or memory, providing:
 * - Song playback with varispeed control (0.25x to 2.0x)
 * - Note and segment preview for editing
 * - Instrument and sample preview
 * - Real-time DSP channel state monitoring
 *
 * Playback uses direct SPC700/DSP emulation for authentic SNES audio.
 */
class MusicPlayer {
 public:
  explicit MusicPlayer(zelda3::music::MusicBank* music_bank);
  ~MusicPlayer();

  // === Dependency Injection ===
  void SetRom(Rom* rom);

  /**
   * @brief Set the main emulator instance to use for playback.
   *
   * MusicPlayer controls this emulator directly for audio playback.
   *
   * @param emulator The emulator instance (must outlive MusicPlayer)
   */
  void SetEmulator(emu::Emulator* emulator);

  /**
   * @brief Set a callback to be called when audio playback starts/stops.
   *
   * This allows external systems (like EditorManager) to pause/mute other
   * audio sources (like the main emulator) when MusicPlayer takes control.
   *
   * @param callback Function called with (true) when playback starts,
   *                 and (false) when playback stops.
   */
  void SetAudioExclusivityCallback(std::function<void(bool)> callback) {
    audio_exclusivity_callback_ = std::move(callback);
  }

  // Access the emulator
  emu::Emulator* emulator() { return emulator_; }

  // === Main Update Loop ===
  /**
   * @brief Call once per frame to update playback state.
   * 
   * This polls the emulator for current song info and updates timing state.
   * Note: The actual audio processing is done by the emulator's RunAudioFrame().
   */
  void Update();

  // === Playback Control ===
  /**
   * @brief Start playing a song by index.
   * @param song_index Zero-based song index in the music bank.
   */
  void PlaySong(int song_index);

  /**
   * @brief Pause the current playback.
   */
  void Pause();

  /**
   * @brief Resume paused playback.
   */
  void Resume();

  /**
   * @brief Stop playback completely.
   */
  void Stop();

  /**
   * @brief Toggle between play/pause states.
   */
  void TogglePlayPause();

  // === Preview Methods ===
  /**
   * @brief Preview a single note with the current instrument.
   */
  void PreviewNote(const zelda3::music::MusicSong& song,
                   const zelda3::music::TrackEvent& event, int segment_index,
                   int channel_index);

  /**
   * @brief Preview a specific segment of a song.
   */
  void PreviewSegment(const zelda3::music::MusicSong& song, int segment_index);

  /**
   * @brief Preview an instrument at middle C.
   */
  void PreviewInstrument(int instrument_index);

  /**
   * @brief Preview a raw BRR sample.
   */
  void PreviewSample(int sample_index);

  /**
   * @brief Preview a custom (modified) song from memory.
   */
  void PreviewCustomSong(int song_index);

  // === Configuration ===
  /**
   * @brief Set the master volume (0.0 to 1.0).
   */
  void SetVolume(float volume);

  /**
   * @brief Set the playback speed (0.25x to 2.0x).
   * 
   * This affects both tempo and pitch (tape-style varispeed).
   */
  void SetPlaybackSpeed(float speed);

  /**
   * @brief Set the DSP interpolation type for audio quality.
   * @param type 0=Linear, 1=Hermite, 2=Gaussian, 3=Cosine, 4=Cubic
   */
  void SetInterpolationType(int type);

  /**
   * @brief Enable/disable direct SPC mode (bypasses game CPU).
   */
  void SetDirectSpcMode(bool enabled);

  /**
   * @brief Seek to a specific segment in the current song.
   */
  void SeekToSegment(int segment_index);

  // === State Access ===
  PlaybackState GetState() const;
  PlaybackMode GetMode() const { return mode_; }
  ChannelState GetChannelState(int channel_index) const;
  std::array<ChannelState, 8> GetChannelStates() const;

  /**
   * @brief Check if the audio system is ready for playback.
   */
  bool IsAudioReady() const;

  /**
   * @brief Check if currently playing.
   */
  bool IsPlaying() const { return mode_ == PlaybackMode::Playing; }

  /**
   * @brief Check if currently paused.
   */
  bool IsPaused() const { return mode_ == PlaybackMode::Paused; }

  /**
   * @brief Get the index of the currently playing song, or -1 if none.
   */
  int GetPlayingSongIndex() const { return playing_song_index_; }

  /**
   * @brief Resolve the instrument used at a specific tick in a track.
   */
  const zelda3::music::MusicInstrument* ResolveInstrumentForEvent(
      const zelda3::music::MusicSegment& segment, int channel_index,
      uint16_t tick) const;

  // === Debug Diagnostics ===
  /**
   * @brief Get DSP buffer diagnostic status.
   */
  DspDebugStatus GetDspStatus() const;

  /**
   * @brief Get APU timing diagnostic status.
   */
  ApuDebugStatus GetApuStatus() const;

  /**
   * @brief Get audio queue diagnostic status.
   */
  AudioQueueStatus GetAudioQueueStatus() const;

  // === Debug Actions ===
  /**
   * @brief Clear the audio queue (stops sound immediately).
   */
  void ClearAudioQueue();

  /**
   * @brief Reset the DSP sample buffer.
   */
  void ResetDspBuffer();

  /**
   * @brief Force a DSP NewFrame() call.
   */
  void ForceNewFrame();

  /**
   * @brief Reinitialize the audio system.
   */
  void ReinitAudio();

 private:
  // === Internal Helpers ===
  bool EnsureAudioReady();
  bool EnsurePreviewReady();
  void InitializeDirectSpc();
  void InitializePreviewMode();
  void PlaySongDirect(int song_id);
  void UploadSoundBankFromRom(uint32_t rom_offset);
  void UploadSongToAram(const std::vector<uint8_t>& data, uint16_t aram_address);
  uint32_t GetBankRomOffset(uint8_t bank) const;
  int GetSongIndexInBank(int song_id, uint8_t bank) const;
  float CalculateTicksPerSecond(uint8_t tempo) const;
  uint32_t GetCurrentPlaybackTick() const;
  uint8_t GetSongTempo(const zelda3::music::MusicSong& song) const;
  void TransitionTo(PlaybackMode new_mode);

  /**
   * @brief Prepare audio pipeline for playback.
   *
   * Consolidates the common audio priming pattern used by all playback methods:
   * - Resets DSP sample buffer
   * - Runs one audio frame to generate initial samples
   * - Queues initial samples to audio backend
   * - Starts audio playback and emulator
   */
  void PrepareAudioPlayback();

  // === Dependencies ===
  zelda3::music::MusicBank* music_bank_ = nullptr;
  emu::Emulator* emulator_ = nullptr;  // Injected main emulator
  Rom* rom_ = nullptr;

  // === Playback State ===
  PlaybackMode mode_ = PlaybackMode::Stopped;
  int playing_song_index_ = -1;

  // === Playback Settings ===
  bool use_direct_spc_ = true;
  float volume_ = 1.0f;
  int interpolation_type_ = 2;  // Gaussian (authentic SNES)

  // === Internal State ===
  bool spc_initialized_ = false;
  bool preview_initialized_ = false;
  uint8_t current_spc_bank_ = 0xFF;

  // === Timing ===
  std::chrono::steady_clock::time_point playback_start_time_;
  std::chrono::steady_clock::time_point last_frame_time_;  // Frame pacing for Update()
  uint32_t playback_start_tick_ = 0;
  float ticks_per_second_ = 0.0f;
  int playback_segment_index_ = 0;

  // === Callback for audio exclusivity ===
  std::function<void(bool)> audio_exclusivity_callback_;
};

}  // namespace music
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_MUSIC_PLAYER_H
