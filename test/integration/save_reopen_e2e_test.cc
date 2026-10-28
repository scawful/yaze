// End-to-end save/reopen checks that run without a real ROM.
//
// Each case builds a synthetic ROM, edits it through a production entry point
// (EditorManager, the vanilla message writer, or a separate z3ed process),
// saves it to disk, reloads it from the file in a fresh object or process, and
// asserts both that the edit persisted and that every other byte is unchanged.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor_manager.h"
#include "app/editor/message/message_data.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/startup_flags.h"
#include "core/features.h"
#include "editor_test_support.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "testing.h"
#include "unique_temp_path.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

#include "imgui/imgui.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace yaze::test {
namespace {

namespace fs = std::filesystem;

// Synthetic dungeon layout. Room 0 owns its header record so an edit to it
// cannot be refused as a shared-header write; every other room points at a
// second record that must stay untouched.
constexpr int kRoomHeaderTablePc = 0x020000;
constexpr int kRoom0HeaderPc = 0x021000;
constexpr int kOtherRoomsHeaderPc = 0x021010;
constexpr int kRoomHeaderSize = 14;
constexpr uint8_t kRoom0Header[kRoomHeaderSize] = {0x20, 0x11, 0x22, 0x33, 0x00,
                                                   0x00, 0x00, 0x00, 0x00, 0x00,
                                                   0x00, 0x00, 0x00, 0x00};
constexpr uint8_t kOtherRoomsHeader[kRoomHeaderSize] = {
    0x20, 0x44, 0x55, 0x66, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

struct ScopedTempDir {
  ScopedTempDir() : path(UniqueTempPath("yaze_save_reopen_e2e")) {
    fs::create_directories(path);
  }
  ~ScopedTempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  fs::path path;
};

struct FeatureFlagsGuard {
  core::FeatureFlags::Flags saved = core::FeatureFlags::get();
  ~FeatureFlagsGuard() { core::FeatureFlags::get() = saved; }
};

struct ScopedImGuiContext {
  ScopedImGuiContext() : ctx(ImGui::CreateContext()) {
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~ScopedImGuiContext() { ImGui::DestroyContext(ctx); }
  ImGuiContext* ctx;
};

std::vector<uint8_t> ReadFileBytes(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  EXPECT_TRUE(input.is_open()) << path;
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

std::string ReadFileText(const fs::path& path) {
  const auto bytes = ReadFileBytes(path);
  return std::string(bytes.begin(), bytes.end());
}

void WriteFileBytes(const fs::path& path, const std::vector<uint8_t>& data) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open()) << path;
  output.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
  ASSERT_TRUE(output.good()) << path;
}

// Returns the offsets at which `before` and `after` differ. Sizes must match.
std::vector<size_t> ChangedOffsets(const std::vector<uint8_t>& before,
                                   const std::vector<uint8_t>& after) {
  EXPECT_EQ(before.size(), after.size());
  std::vector<size_t> changed;
  const size_t size = std::min(before.size(), after.size());
  for (size_t i = 0; i < size; ++i) {
    if (before[i] != after[i]) {
      changed.push_back(i);
    }
  }
  return changed;
}

void WriteLong(std::vector<uint8_t>* data, int pc, uint32_t value) {
  (*data)[pc] = value & 0xFF;
  (*data)[pc + 1] = (value >> 8) & 0xFF;
  (*data)[pc + 2] = (value >> 16) & 0xFF;
}

std::vector<uint8_t> MakeDungeonHeaderRom() {
  std::vector<uint8_t> data(0x100000, 0x00);
  const std::string title = "YAZE SAVE REOPEN E2E";
  std::copy(title.begin(), title.end(), data.begin() + 0x7FC0);

  WriteLong(&data, zelda3::kRoomHeaderPointer, PcToSnes(kRoomHeaderTablePc));
  const uint32_t room0_snes = PcToSnes(kRoom0HeaderPc);
  const uint32_t other_snes = PcToSnes(kOtherRoomsHeaderPc);
  data[zelda3::kRoomHeaderPointerBank] = (room0_snes >> 16) & 0xFF;
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    const uint32_t snes = room_id == 0 ? room0_snes : other_snes;
    data[kRoomHeaderTablePc + room_id * 2] = snes & 0xFF;
    data[kRoomHeaderTablePc + room_id * 2 + 1] = (snes >> 8) & 0xFF;
  }
  std::copy(std::begin(kRoom0Header), std::end(kRoom0Header),
            data.begin() + kRoom0HeaderPc);
  std::copy(std::begin(kOtherRoomsHeader), std::end(kOtherRoomsHeader),
            data.begin() + kOtherRoomsHeaderPc);
  return data;
}

// Opens a synthetic ROM in EditorManager with the save flags a ROM open
// selects, edits room 0's header through the dungeon editor's room store,
// saves, and reloads the file in a fresh Rom. A save must write only the
// edited header bytes; any other saver that rewrites unloaded or unrelated
// data on a dungeon-only save fails the byte comparison.
TEST(SaveReopenE2ETest, DungeonHeaderEditSavesOnlyItsBytesAndReopens) {
  FeatureFlagsGuard flags_guard;
  ScopedImGuiContext imgui;
  ScopedTempDir temp;
  const fs::path rom_path = temp.path / "dungeon.sfc";
  const std::vector<uint8_t> original = MakeDungeonHeaderRom();
  WriteFileBytes(rom_path, original);

  gfx::NullRenderer renderer;
  auto manager = std::make_unique<editor::EditorManager>();
  InitializeWithIsolatedSettings(*manager, &renderer);
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  // The blocks saver refuses ROMs without the vanilla bank 02 loader code,
  // which a synthetic ROM cannot provide. Saving messages loads all graphics
  // sheets for the message editor, which takes seconds and writes nothing
  // here; VanillaMessageEditsSurviveSaveAndFreshLoad covers message saves.
  core::FeatureFlags::get().dungeon.kSaveBlocks = false;
  core::FeatureFlags::get().kSaveMessages = false;

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* dungeon = editor_set->GetEditorAs<editor::DungeonEditorV2>(
      editor::EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);

  // DungeonRoomLoader populates the store with this header loader.
  auto& room = dungeon->rooms()[0];
  room = zelda3::LoadRoomHeaderFromRom(rom, 0);
  ASSERT_EQ(room.palette(), 0x11);
  ASSERT_EQ(room.blockset(), 0x22);
  ASSERT_EQ(room.spriteset(), 0x33);
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  room.SetBlockset(0x07);
  room.SetSpriteset(0x0B);
  ASSERT_TRUE(room.header_dirty());

  ASSERT_OK(manager->SaveRom());
  EXPECT_FALSE(room.header_dirty());
  manager.reset();

  const std::vector<uint8_t> saved = ReadFileBytes(rom_path);
  ASSERT_EQ(saved.size(), original.size());
  EXPECT_EQ(ChangedOffsets(original, saved),
            (std::vector<size_t>{kRoom0HeaderPc + 1, kRoom0HeaderPc + 2,
                                 kRoom0HeaderPc + 3}));

  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(rom_path.string()));
  const zelda3::Room room0 = zelda3::LoadRoomHeaderFromRom(&reopened, 0);
  EXPECT_EQ(room0.palette(), 0x2A);
  EXPECT_EQ(room0.blockset(), 0x07);
  EXPECT_EQ(room0.spriteset(), 0x0B);
  const zelda3::Room room1 = zelda3::LoadRoomHeaderFromRom(&reopened, 1);
  EXPECT_EQ(room1.palette(), 0x44);
  EXPECT_EQ(room1.blockset(), 0x55);
  EXPECT_EQ(room1.spriteset(), 0x66);
}

std::vector<uint8_t> EncodeMessage(const std::string& text) {
  const auto parsed = editor::ParseMessageToDataWithDiagnostics(text);
  EXPECT_TRUE(parsed.ok()) << text;
  return parsed.bytes;
}

std::vector<editor::MessageData> MakeMessages(
    const std::vector<std::string>& texts) {
  std::vector<editor::MessageData> messages;
  for (size_t index = 0; index < texts.size(); ++index) {
    editor::MessageData message;
    message.ID = static_cast<int>(index);
    message.Data = EncodeMessage(texts[index]);
    messages.push_back(std::move(message));
  }
  return messages;
}

std::vector<std::string> ReadVanillaMessageTexts(Rom& rom) {
  const auto messages = editor::ReadAllTextData(
      rom.mutable_data(), editor::kTextData, static_cast<int>(rom.size()));
  std::vector<std::string> texts;
  for (const auto& message : messages) {
    texts.push_back(message.RawString);
  }
  return texts;
}

// Writes vanilla messages that span both text banks, saves, reopens from disk,
// edits a message on each side of the bank switch so later messages move, then
// saves and reopens again. The reader and writer must agree through the file:
// a [W:80] argument is not a bank switch, and nothing outside the two text
// banks may change.
TEST(SaveReopenE2ETest, VanillaMessageEditsSurviveSaveAndFreshLoad) {
  ScopedTempDir temp;
  const fs::path original_path = temp.path / "messages.sfc";
  const fs::path edited_path = temp.path / "messages-edited.sfc";
  const std::vector<std::string> original_texts = {"HELLO", "[W:80]Wait[BANK]",
                                                   "Zelda", "Link"};

  {
    Rom rom;
    ASSERT_OK(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0xA5)));
    ASSERT_OK(editor::WriteAllTextData(&rom, MakeMessages(original_texts)));
    Rom::SaveSettings settings;
    settings.filename = original_path.string();
    ASSERT_OK(rom.SaveToFile(settings));
  }
  const std::vector<uint8_t> original_bytes = ReadFileBytes(original_path);

  const std::vector<std::string> edited_texts = {"Hi", "[W:80]Wait[BANK]",
                                                 "Princess Zelda", "Link"};
  std::vector<std::pair<uint32_t, uint32_t>> write_ranges;
  {
    Rom rom;
    ASSERT_OK(rom.LoadFromFile(original_path.string()));
    ASSERT_EQ(ReadVanillaMessageTexts(rom), original_texts);

    const auto edited = MakeMessages(edited_texts);
    auto plan = editor::BuildVanillaMessageSavePlan(edited, edited.size());
    ASSERT_OK(plan.status());
    write_ranges = plan->write_ranges();
    ASSERT_OK(editor::ApplyVanillaMessageSavePlan(&rom, *plan));
    Rom::SaveSettings settings;
    settings.filename = edited_path.string();
    ASSERT_OK(rom.SaveToFile(settings));
  }

  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(edited_path.string()));
  EXPECT_EQ(ReadVanillaMessageTexts(reopened), edited_texts);

  const std::vector<uint8_t> edited_bytes = ReadFileBytes(edited_path);
  for (const size_t offset : ChangedOffsets(original_bytes, edited_bytes)) {
    const bool planned = std::any_of(
        write_ranges.begin(), write_ranges.end(), [offset](const auto& range) {
          return offset >= range.first && offset < range.second;
        });
    EXPECT_TRUE(planned) << "unplanned change at PC 0x" << std::hex << offset;
  }
}

#if defined(YAZE_TEST_Z3ED_PATH)

struct ProcessResult {
  int exit_code = -1;
  std::string out;
  std::string err;
};

std::string Quote(const std::string& value) {
  return absl::StrCat("\"", value, "\"");
}

// Runs z3ed in its own process with stdout and stderr captured to files.
ProcessResult RunZ3ed(const fs::path& work_dir,
                      const std::vector<std::string>& args) {
  const fs::path out_path = work_dir / "z3ed.out";
  const fs::path err_path = work_dir / "z3ed.err";
  std::string command =
      Quote(fs::path(YAZE_TEST_Z3ED_PATH).make_preferred().string());
  for (const auto& arg : args) {
    absl::StrAppend(&command, " ", Quote(arg));
  }
  absl::StrAppend(&command, " > ", Quote(out_path.string()), " 2> ",
                  Quote(err_path.string()));
#if defined(_WIN32)
  // cmd.exe strips the first and last quote of a command that starts with one.
  command = Quote(command);
#endif
  ProcessResult result;
  const int status = std::system(command.c_str());
#if defined(_WIN32)
  result.exit_code = status;
#else
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
  result.out = ReadFileText(out_path);
  result.err = ReadFileText(err_path);
  return result;
}

nlohmann::json ParseJsonOutput(const ProcessResult& result) {
  auto json = nlohmann::json::parse(result.out, nullptr,
                                    /*allow_exceptions=*/false);
  EXPECT_FALSE(json.is_discarded()) << result.out << "\n" << result.err;
  return json;
}

std::vector<fs::path> FindBackups(const fs::path& rom_path) {
  const std::string prefix = rom_path.filename().string() + "_backup_";
  std::vector<fs::path> backups;
  for (const auto& entry : fs::directory_iterator(rom_path.parent_path())) {
    if (entry.path().filename().string().rfind(prefix, 0) == 0) {
      backups.push_back(entry.path());
    }
  }
  return backups;
}

// One z3ed process edits a room header and saves; a second process reopens
// the file and reads it back. This is the CLI open/save/reopen check from the
// release smoke, run on every CI platform. It fails if the CLI reports a
// write it never persisted, drops --rom, or backs up the wrong file.
TEST(SaveReopenE2ETest, Z3edRoomPropertyWritePersistsAcrossProcesses) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path / "dungeon.sfc";
  const std::vector<uint8_t> original = MakeDungeonHeaderRom();
  WriteFileBytes(rom_path, original);
  const std::string rom_arg = "--rom=" + rom_path.string();

  const ProcessResult write = RunZ3ed(
      temp.path, {"dungeon-set-room-property", rom_arg, "--room=0x00",
                  "--property=palette", "--value=0x2A", "--format=json"});
  ASSERT_EQ(write.exit_code, 0) << write.out << "\n" << write.err;
  const auto write_json = ParseJsonOutput(write);
  ASSERT_TRUE(write_json.contains("Dungeon Room Property Set")) << write.out;
  EXPECT_EQ(write_json["Dungeon Room Property Set"].value("save_status", ""),
            "saved");

  const std::vector<uint8_t> saved = ReadFileBytes(rom_path);
  EXPECT_EQ(ChangedOffsets(original, saved),
            (std::vector<size_t>{kRoom0HeaderPc + 1}));
  EXPECT_EQ(saved[kRoom0HeaderPc + 1], 0x2A);
  const auto backups = FindBackups(rom_path);
  ASSERT_EQ(backups.size(), 1u);
  EXPECT_EQ(ReadFileBytes(backups.front()), original);

  const ProcessResult read = RunZ3ed(
      temp.path, {"dungeon-room-header", rom_arg, "--room=0", "--format=json"});
  ASSERT_EQ(read.exit_code, 0) << read.out << "\n" << read.err;
  const auto read_json = ParseJsonOutput(read);
  ASSERT_TRUE(read_json.contains("Room Header Debug")) << read.out;
  const auto& decoded = read_json["Room Header Debug"]["decoded"];
  EXPECT_EQ(decoded.value("palette", -1), 0x2A);
  EXPECT_EQ(decoded.value("blockset", -1), 0x22);
  EXPECT_EQ(decoded.value("spriteset", -1), 0x33);
}

#endif  // defined(YAZE_TEST_Z3ED_PATH)

}  // namespace
}  // namespace yaze::test
