#include "app/emu/emulator.h"

#include <cstdint>
#include <fstream>
#include <vector>

#include "app/core/window.h"
#include "util/log.h"

namespace yaze::core {
  extern bool g_window_is_resizing;
}

#include "app/emu/debug/disassembly_viewer.h"
#include "app/emu/ui/debugger_ui.h"
#include "app/emu/ui/emulator_ui.h"
#include "app/emu/ui/input_handler.h"
#include "app/gui/color.h"
#include "app/gui/editor_layout.h"
#include "app/gui/icons.h"
#include "app/gui/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace emu {

Emulator::~Emulator() {
  // Don't call Cleanup() in destructor - renderer is already destroyed
  // Just stop emulation
  running_ = false;
}

void Emulator::Cleanup() {
  // Stop emulation
  running_ = false;
  
  // Don't try to destroy PPU texture during shutdown
  // The renderer is destroyed before the emulator, so attempting to
  // call renderer_->DestroyTexture() will crash
  // The texture will be cleaned up automatically when SDL quits
  ppu_texture_ = nullptr;
  
  // Reset state
  snes_initialized_ = false;
}

void Emulator::Initialize(gfx::IRenderer* renderer, const std::vector<uint8_t>& rom_data) {
  // This method is now optional - emulator can be initialized lazily in Run()
  renderer_ = renderer;
  rom_data_ = rom_data;
  
  // Register emulator cards with EditorCardManager
  auto& card_manager = gui::EditorCardManager::Get();
  
  card_manager.RegisterCard({
      .card_id = "emulator.cpu_debugger",
      .display_name = "CPU Debugger",
      .icon = ICON_MD_BUG_REPORT,
      .category = "Emulator",
      .visibility_flag = &show_cpu_debugger_,
      .priority = 10
  });
  
  card_manager.RegisterCard({
      .card_id = "emulator.memory_viewer",
      .display_name = "Memory Viewer",
      .icon = ICON_MD_MEMORY,
      .category = "Emulator",
      .visibility_flag = &show_memory_viewer_,
      .priority = 20
  });
  
  card_manager.RegisterCard({
      .card_id = "emulator.ppu_viewer",
      .display_name = "PPU Viewer",
      .icon = ICON_MD_GRID_VIEW,
      .category = "Emulator",
      .visibility_flag = &show_ppu_viewer_,
      .priority = 30
  });
  
  card_manager.RegisterCard({
      .card_id = "emulator.audio_mixer",
      .display_name = "Audio Mixer",
      .icon = ICON_MD_AUDIO_FILE,
      .category = "Emulator",
      .visibility_flag = &show_audio_mixer_,
      .priority = 40
  });
  
  printf("[Emulator] Registered 4 cards with EditorCardManager\n");
  
  // Reset state for new ROM
  running_ = false;
  snes_initialized_ = false;
  
  // Initialize audio backend if not already done
  if (!audio_backend_) {
    audio_backend_ = audio::AudioBackendFactory::Create(
        audio::AudioBackendFactory::BackendType::SDL2);

    audio::AudioConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    // Use moderate buffer size - 1024 samples = ~21ms latency
    // This is a good balance between latency and stability
    config.buffer_frames = 1024;
    config.format = audio::SampleFormat::INT16;

    if (!audio_backend_->Initialize(config)) {
      LOG_ERROR("Emulator", "Failed to initialize audio backend");
    } else {
      LOG_INFO("Emulator", "Audio backend initialized: %s",
               audio_backend_->GetBackendName().c_str());
    }
  }
  
  // Set up CPU breakpoint callback
  snes_.cpu().on_breakpoint_hit_ = [this](uint32_t pc) -> bool {
    return breakpoint_manager_.ShouldBreakOnExecute(pc, BreakpointManager::CpuType::CPU_65816);
  };
  
  // Set up instruction recording callback for DisassemblyViewer
  snes_.cpu().on_instruction_executed_ = [this](uint32_t address, uint8_t opcode,
                                                const std::vector<uint8_t>& operands,
                                                const std::string& mnemonic,
                                                const std::string& operand_str) {
    disassembly_viewer_.RecordInstruction(address, opcode, operands, mnemonic, operand_str);
  };
  
  initialized_ = true;
}

void Emulator::Run(Rom* rom) {
  // Lazy initialization: set renderer from Controller if not set yet
  if (!renderer_) {
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 
                       "Emulator renderer not initialized");
    return;
  }
  
  // Initialize audio backend if not already done (lazy initialization)
  if (!audio_backend_) {
    audio_backend_ = audio::AudioBackendFactory::Create(
        audio::AudioBackendFactory::BackendType::SDL2);

    audio::AudioConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    // Use moderate buffer size - 1024 samples = ~21ms latency
    // This is a good balance between latency and stability
    config.buffer_frames = 1024;
    config.format = audio::SampleFormat::INT16;

    if (!audio_backend_->Initialize(config)) {
      LOG_ERROR("Emulator", "Failed to initialize audio backend");
    } else {
      LOG_INFO("Emulator", "Audio backend initialized (lazy): %s",
               audio_backend_->GetBackendName().c_str());
    }
  }
  
  // Initialize input manager if not already done
  if (!input_manager_.IsInitialized()) {
    if (!input_manager_.Initialize(input::InputBackendFactory::BackendType::SDL2)) {
      LOG_ERROR("Emulator", "Failed to initialize input manager");
    } else {
      LOG_INFO("Emulator", "Input manager initialized: %s",
               input_manager_.backend()->GetBackendName().c_str());
    }
  }
  
  // Initialize SNES and create PPU texture on first run
  // This happens lazily when user opens the emulator window
  if (!snes_initialized_ && rom->is_loaded()) {
    // Create PPU texture with correct format for SNES emulator
    // ARGB8888 matches the XBGR format used by the SNES PPU (pixel format 1)
    if (!ppu_texture_) {
      ppu_texture_ = renderer_->CreateTextureWithFormat(
          512, 480, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING);
      if (ppu_texture_ == NULL) {
        printf("Failed to create PPU texture: %s\n", SDL_GetError());
        return;
      }
    }

    // Initialize SNES with ROM data (either from Initialize() or from rom parameter)
    if (rom_data_.empty()) {
      rom_data_ = rom->vector();
    }
    snes_.Init(rom_data_);
    
    // Note: DisassemblyViewer recording is always enabled via callback
    // No explicit setup needed - callback is set in Initialize()

    // Note: PPU pixel format set to 1 (XBGR) in Init() which matches ARGB8888 texture

    wanted_frames_ = 1.0 / (snes_.memory().pal_timing() ? 50.0 : 60.0);
    wanted_samples_ = 48000 / (snes_.memory().pal_timing() ? 50 : 60);
    snes_initialized_ = true;

    count_frequency = SDL_GetPerformanceFrequency();
    last_count = SDL_GetPerformanceCounter();
    time_adder = 0.0;
    frame_count_ = 0;
    fps_timer_ = 0.0;
    current_fps_ = 0.0;
    
    // Start emulator in running state by default
    // User can press Space to pause if needed
    running_ = true;
  }

  RenderNavBar();

  // Auto-pause emulator during window resize to prevent crashes
  // MODERN APPROACH: Only pause on actual window resize, not focus loss
  static bool was_running_before_resize = false;

  // Check if window is being resized (set in HandleEvents)
  if (yaze::core::g_window_is_resizing && running_) {
    was_running_before_resize = true;
    running_ = false;
  } else if (!yaze::core::g_window_is_resizing && !running_ && was_running_before_resize) {
    // Auto-resume after resize completes
    running_ = true;
    was_running_before_resize = false;
  }

  // REMOVED: Aggressive focus-based pausing
  // Modern emulators (RetroArch, bsnes, etc.) continue running in background
  // Users can manually pause with Space if they want to save CPU/battery

  if (running_) {
    // Poll input and update SNES controller state
    input_manager_.Poll(&snes_, 1);  // Player 1

    uint64_t current_count = SDL_GetPerformanceCounter();
    uint64_t delta = current_count - last_count;
    last_count = current_count;
    double seconds = delta / (double)count_frequency;
    time_adder += seconds;

    // Cap time accumulation to prevent spiral of death and improve stability
    if (time_adder > wanted_frames_ * 3.0) {
      time_adder = wanted_frames_ * 3.0;
    }

    // Track frames to skip for performance
    int frames_to_process = 0;
    while (time_adder >= wanted_frames_ - 0.002) {
      time_adder -= wanted_frames_;
      frames_to_process++;
    }

    // Limit maximum frames to process (prevent spiral of death)
    if (frames_to_process > 4) {
      frames_to_process = 4;
    }

    if (snes_initialized_ && frames_to_process > 0) {
      // Process frames (skip rendering for all but last frame if falling behind)
      for (int i = 0; i < frames_to_process; i++) {
        bool should_render = (i == frames_to_process - 1);

        // Run frame
        if (turbo_mode_) {
          snes_.RunFrame();
        }
        snes_.RunFrame();

        // Track FPS
        frame_count_++;
        fps_timer_ += wanted_frames_;
        if (fps_timer_ >= 1.0) {
          current_fps_ = frame_count_ / fps_timer_;
          frame_count_ = 0;
          fps_timer_ = 0.0;
        }

        // Only render and handle audio on the last frame
        if (should_render) {
          // ADAPTIVE AUDIO BUFFERING
          // Target: Keep 2-3 frames worth of audio queued for smooth playback
          // This prevents both underruns (crackling) and excessive latency
          
          if (audio_backend_) {
            auto audio_status = audio_backend_->GetStatus();
            uint32_t queued_frames = audio_status.queued_frames;
            
            // Target buffer: 2.5 frames (2000 samples at 48kHz/60fps)
            // Min: 1.5 frames (1200 samples) - below this we need to queue
            // Max: 4.0 frames (3200 samples) - above this we skip queueing
            const uint32_t target_buffer = wanted_samples_ * 2.5;
            const uint32_t min_buffer = wanted_samples_ * 1.5;
            const uint32_t max_buffer = wanted_samples_ * 4.0;
            
            bool should_queue = (queued_frames < max_buffer);
            
            if (should_queue) {
              // Generate samples from SNES APU/DSP
              snes_.SetSamples(audio_buffer_, wanted_samples_);
              
              int num_samples = wanted_samples_ * 2;  // Stereo (L+R channels)
              
              // Adaptive sample adjustment to prevent drift
              // Note: We can only queue up to what we generated
              if (queued_frames < min_buffer) {
                // Buffer running low - queue all samples we have
                // In future frames this helps catch up
                num_samples = wanted_samples_ * 2;
              } else if (queued_frames > target_buffer) {
                // Buffer too high - queue fewer samples to drain faster
                // Drop some samples to reduce latency
                num_samples = static_cast<int>(wanted_samples_ * 2 * 0.8);
                if (num_samples < wanted_samples_ * 2 * 0.5) {
                  num_samples = wanted_samples_ * 2 * 0.5;  // Minimum 50%
                }
              }
              
              if (!audio_backend_->QueueSamples(audio_buffer_, num_samples)) {
                static int error_count = 0;
                if (++error_count % 300 == 0) {
                  LOG_WARN("Emulator", "Failed to queue audio (count: %d)", error_count);
                }
              }
            }
            
            // AUDIO DEBUG: Compact per-frame monitoring
            static int audio_debug_counter = 0;
            audio_debug_counter++;
            
            // Log first 10 frames, then every 60 frames for ongoing monitoring
            if (audio_debug_counter < 10 || audio_debug_counter % 60 == 0) {
              printf("[AUDIO] Frame %d: Queued=%u Target=%u Status=%s\n",
                     audio_debug_counter, queued_frames, target_buffer,
                     should_queue ? "QUEUE" : "SKIP");
            }
          }

          // Update PPU texture only on rendered frames
          void* ppu_pixels_;
          int ppu_pitch_;
          if (renderer_->LockTexture(ppu_texture_, NULL, &ppu_pixels_, &ppu_pitch_)) {
            snes_.SetPixels(static_cast<uint8_t*>(ppu_pixels_));
            renderer_->UnlockTexture(ppu_texture_);
            
            // WORKAROUND: Tiny delay after texture unlock to prevent macOS Metal crash
            // macOS CoreAnimation/Metal driver bug in layer_presented() callback
            // Without this, rapid texture updates corrupt Metal's frame tracking
            SDL_Delay(1);
          }
        }
      }
    }
  }

  RenderEmulatorInterface();
}

void Emulator::RenderEmulatorInterface() {
  // Apply modern theming with safety checks
  try {
    auto& theme_manager = gui::ThemeManager::Get();
    const auto& theme = theme_manager.GetCurrentTheme();

    // Modern EditorCard-based layout - modular and flexible
    static bool show_cpu_debugger_ = true;
    static bool show_ppu_display_ = true;
    static bool show_memory_viewer_ = false;
    static bool show_breakpoints_ = false;
    static bool show_performance_ = true;
    static bool show_ai_agent_ = false;
    static bool show_save_states_ = false;
    static bool show_keyboard_config_ = false;
    static bool show_apu_debugger_ = true;

    // Create session-aware cards
    static gui::EditorCard cpu_card(ICON_MD_MEMORY " CPU Debugger", ICON_MD_MEMORY);
    static gui::EditorCard ppu_card(ICON_MD_VIDEOGAME_ASSET " PPU Display",
                             ICON_MD_VIDEOGAME_ASSET);
    static gui::EditorCard memory_card(ICON_MD_DATA_ARRAY " Memory Viewer",
                                ICON_MD_DATA_ARRAY);
    static gui::EditorCard breakpoints_card(ICON_MD_BUG_REPORT " Breakpoints",
                                     ICON_MD_BUG_REPORT);
    static gui::EditorCard performance_card(ICON_MD_SPEED " Performance",
                                     ICON_MD_SPEED);
    static gui::EditorCard ai_card(ICON_MD_SMART_TOY " AI Agent", ICON_MD_SMART_TOY);
    static gui::EditorCard save_states_card(ICON_MD_SAVE " Save States", ICON_MD_SAVE);
    static gui::EditorCard keyboard_card(ICON_MD_KEYBOARD " Keyboard Config",
                                  ICON_MD_KEYBOARD);
    static gui::EditorCard apu_debug_card(ICON_MD_MUSIC_NOTE " APU Debugger",
                                   ICON_MD_MUSIC_NOTE);

    // Configure default positions
    static bool cards_configured = false;
    if (!cards_configured) {
      cpu_card.SetDefaultSize(400, 500);
      cpu_card.SetPosition(gui::EditorCard::Position::Right);

      ppu_card.SetDefaultSize(550, 520);
      ppu_card.SetPosition(gui::EditorCard::Position::Floating);

      memory_card.SetDefaultSize(800, 600);
      memory_card.SetPosition(gui::EditorCard::Position::Floating);

      breakpoints_card.SetDefaultSize(400, 350);
      breakpoints_card.SetPosition(gui::EditorCard::Position::Right);

      performance_card.SetDefaultSize(350, 300);
      performance_card.SetPosition(gui::EditorCard::Position::Bottom);

      ai_card.SetDefaultSize(500, 450);
      ai_card.SetPosition(gui::EditorCard::Position::Floating);

      save_states_card.SetDefaultSize(400, 300);
      save_states_card.SetPosition(gui::EditorCard::Position::Floating);

      keyboard_card.SetDefaultSize(450, 400);
      keyboard_card.SetPosition(gui::EditorCard::Position::Floating);

      apu_debug_card.SetDefaultSize(500, 400);
      apu_debug_card.SetPosition(gui::EditorCard::Position::Floating);

      cards_configured = true;
    }

    // CPU Debugger Card
    if (show_cpu_debugger_) {
      if (cpu_card.Begin(&show_cpu_debugger_)) {
        RenderModernCpuDebugger();
      }
      cpu_card.End();
    }

    // PPU Display Card
    if (show_ppu_display_) {
      if (ppu_card.Begin(&show_ppu_display_)) {
        RenderSnesPpu();
      }
      ppu_card.End();
    }

    // Memory Viewer Card
    if (show_memory_viewer_) {
      if (memory_card.Begin(&show_memory_viewer_)) {
        RenderMemoryViewer();
      }
      memory_card.End();
    }

    // Breakpoints Card
    if (show_breakpoints_) {
      if (breakpoints_card.Begin(&show_breakpoints_)) {
        RenderBreakpointList();
      }
      breakpoints_card.End();
    }

    // Performance Monitor Card
    if (show_performance_) {
      if (performance_card.Begin(&show_performance_)) {
        RenderPerformanceMonitor();
      }
      performance_card.End();
    }

    // AI Agent Card
    if (show_ai_agent_) {
      if (ai_card.Begin(&show_ai_agent_)) {
        RenderAIAgentPanel();
      }
      ai_card.End();
    }

    // Save States Card
    if (show_save_states_) {
      if (save_states_card.Begin(&show_save_states_)) {
        RenderSaveStates();
      }
      save_states_card.End();
    }

    // Keyboard Configuration Card
    if (show_keyboard_config_) {
      if (keyboard_card.Begin(&show_keyboard_config_)) {
        RenderKeyboardConfig();
      }
      keyboard_card.End();
    }

    // APU Debugger Card
    if (show_apu_debugger_) {
      if (apu_debug_card.Begin(&show_apu_debugger_)) {
        RenderApuDebugger();
      }
      apu_debug_card.End();
    }

  } catch (const std::exception& e) {
    // Fallback to basic UI if theming fails
    ImGui::Text("Error loading emulator UI: %s", e.what());
    if (ImGui::Button("Retry")) {
      // Force theme manager reinitialization
      auto& theme_manager = gui::ThemeManager::Get();
      theme_manager.InitializeBuiltInThemes();
    }
  }
}

void Emulator::RenderSnesPpu() {
  // Delegate to UI layer
  ui::RenderSnesPpu(this);
}

void Emulator::RenderNavBar() {
  // Delegate to UI layer
  ui::RenderNavBar(this);
}

// REMOVED: HandleEvents() - replaced by ui::InputHandler::Poll()
// The old ImGui::IsKeyPressed/Released approach was event-based and didn't work properly
// for continuous game input. Now using SDL_GetKeyboardState() for proper polling.

void Emulator::RenderBreakpointList() {
  // Delegate to UI layer
  ui::RenderBreakpointList(this);
}

void Emulator::RenderMemoryViewer() {
  // Delegate to UI layer
  ui::RenderMemoryViewer(this);
}

void Emulator::RenderModernCpuDebugger() {
  try {
    auto& theme_manager = gui::ThemeManager::Get();
    const auto& theme = theme_manager.GetCurrentTheme();
    
    // Debugger controls toolbar
    if (ImGui::Button(ICON_MD_PLAY_ARROW)) { running_ = true; }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_PAUSE)) { running_ = false; }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_SKIP_NEXT " Step")) {
      if (!running_) snes_.cpu().RunOpcode();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_REFRESH)) { snes_.Reset(true); }
    
    ImGui::Separator();
    
    // Breakpoint controls
    static char bp_addr[16] = "00FFD9";
    ImGui::Text(ICON_MD_BUG_REPORT " Breakpoints:");
    ImGui::PushItemWidth(100);
    ImGui::InputText("##BPAddr", bp_addr, IM_ARRAYSIZE(bp_addr),
                    ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_ADD " Add")) {
      uint32_t addr = std::strtoul(bp_addr, nullptr, 16);
      breakpoint_manager_.AddBreakpoint(addr, BreakpointManager::Type::EXECUTE,
                                       BreakpointManager::CpuType::CPU_65816,
                                       "", absl::StrFormat("BP at $%06X", addr));
    }
    
    // List breakpoints
    ImGui::BeginChild("##BPList", ImVec2(0, 100), true);
    for (const auto& bp : breakpoint_manager_.GetAllBreakpoints()) {
      if (bp.cpu == BreakpointManager::CpuType::CPU_65816) {
        bool enabled = bp.enabled;
        if (ImGui::Checkbox(absl::StrFormat("##en%d", bp.id).c_str(), &enabled)) {
          breakpoint_manager_.SetEnabled(bp.id, enabled);
        }
        ImGui::SameLine();
        ImGui::Text("$%06X", bp.address);
        ImGui::SameLine();
        ImGui::TextDisabled("(hits: %d)", bp.hit_count);
        ImGui::SameLine();
        if (ImGui::SmallButton(absl::StrFormat(ICON_MD_DELETE "##%d", bp.id).c_str())) {
          breakpoint_manager_.RemoveBreakpoint(bp.id);
        }
      }
    }
    ImGui::EndChild();
    
    ImGui::Separator();

    ImGui::TextColored(ConvertColorToImVec4(theme.accent), "CPU Status");
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ConvertColorToImVec4(theme.child_bg));
    ImGui::BeginChild("##CpuStatus", ImVec2(0, 200), true);

    // Compact register display in a table
    if (ImGui::BeginTable(
            "Registers", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("A");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().A);
      ImGui::TableNextColumn();
      ImGui::Text("D");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().D);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("X");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().X);
      ImGui::TableNextColumn();
      ImGui::Text("DB");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.cpu().DB);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Y");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().Y);
      ImGui::TableNextColumn();
      ImGui::Text("PB");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.cpu().PB);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("PC");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.success), "0x%04X",
                         snes_.cpu().PC);
      ImGui::TableNextColumn();
      ImGui::Text("SP");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.memory().mutable_sp());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("PS");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.warning), "0x%02X",
                         snes_.cpu().status);
      ImGui::TableNextColumn();
      ImGui::Text("Cycle");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.info), "%llu",
                         snes_.mutable_cycles());

      ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // SPC700 Status Panel
    ImGui::TextColored(ConvertColorToImVec4(theme.accent), "SPC700 Status");
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ConvertColorToImVec4(theme.child_bg));
    ImGui::BeginChild("##SpcStatus", ImVec2(0, 160), true);

    if (ImGui::BeginTable(
            "SPCRegisters", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 50);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 50);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("A");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().A);
      ImGui::TableNextColumn();
      ImGui::Text("PC");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.success), "0x%04X",
                         snes_.apu().spc700().PC);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("X");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().X);
      ImGui::TableNextColumn();
      ImGui::Text("SP");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().SP);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Y");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().Y);
      ImGui::TableNextColumn();
      ImGui::Text("PSW");
      ImGui::TableNextColumn();
      ImGui::TextColored(
          ConvertColorToImVec4(theme.warning), "0x%02X",
          snes_.apu().spc700().FlagsToByte(snes_.apu().spc700().PSW));

      ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // New Disassembly Viewer
    if (ImGui::CollapsingHeader("Disassembly Viewer", 
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      uint32_t current_pc = (static_cast<uint32_t>(snes_.cpu().PB) << 16) | snes_.cpu().PC;
      auto& disasm = snes_.cpu().disassembly_viewer();
      if (disasm.IsAvailable()) {
        disasm.Render(current_pc, snes_.cpu().breakpoints_);
      } else {
        ImGui::TextColored(ConvertColorToImVec4(theme.error), "Disassembly viewer unavailable.");
      }
    }
  } catch (const std::exception& e) {
    // Ensure any pushed styles are popped on error
    try {
      ImGui::PopStyleColor();
    } catch (...) {
      // Ignore PopStyleColor errors
    }
    ImGui::Text("CPU Debugger Error: %s", e.what());
  }
}

void Emulator::RenderPerformanceMonitor() {
  // Delegate to UI layer
  ui::RenderPerformanceMonitor(this);
}

void Emulator::RenderAIAgentPanel() {
  // Delegate to UI layer
  ui::RenderAIAgentPanel(this);
}

void Emulator::RenderCpuInstructionLog(
    const std::vector<InstructionEntry>& instruction_log) {
  // Delegate to UI layer (legacy log deprecated)
  ui::RenderCpuInstructionLog(this, instruction_log.size());
}

void Emulator::RenderSaveStates() {
  // TODO: Create ui::RenderSaveStates() when save state system is implemented
  auto& theme_manager = gui::ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();
  
  ImGui::TextColored(ConvertColorToImVec4(theme.warning), 
                    ICON_MD_SAVE " Save States - Coming Soon");
  ImGui::TextWrapped("Save state functionality will be implemented here.");
}

void Emulator::RenderKeyboardConfig() {
  // Delegate to the input manager UI
  ui::RenderKeyboardConfig(&input_manager_);
}

void Emulator::RenderApuDebugger() {
  // Delegate to UI layer
  ui::RenderApuDebugger(this);
}

}  // namespace emu
}  // namespace yaze
