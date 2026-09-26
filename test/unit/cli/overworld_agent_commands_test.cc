#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "app/service/render_service.h"
#include "cli/handlers/game/overworld_sprite_edit_commands.h"

namespace yaze::cli {
namespace {

std::filesystem::path UniqueDir(const std::string& tag) {
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("yaze_ow_agent_" + tag + "_" + std::to_string(stamp));
}

TEST(OverworldSpriteEditGuardTest, DetectsRomInsideOracleCheckout) {
  const auto root = UniqueDir("guard");
  std::filesystem::create_directories(root / "checkout" / "Roms");
  std::filesystem::create_directories(root / "copy" / "Roms");
  std::ofstream(root / "checkout" / "Oracle_main.asm") << "; main\n";
  std::ofstream(root / "checkout" / "Roms" / "oos168.sfc") << "rom";
  std::ofstream(root / "copy" / "Roms" / "oos168.sfc") << "rom";

  EXPECT_TRUE(handlers::IsOracleProjectRomPath(root / "checkout" / "Roms" /
                                               "oos168.sfc"));
  // Relative-looking paths with .. still resolve to the checkout.
  EXPECT_TRUE(handlers::IsOracleProjectRomPath(
      root / "copy" / ".." / "checkout" / "Roms" / "oos168.sfc"));
  EXPECT_FALSE(
      handlers::IsOracleProjectRomPath(root / "copy" / "Roms" / "oos168.sfc"));
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
}

TEST(OverworldRenderServiceTest, RejectsBadRequestsBeforeLoading) {
  app::service::RenderService service(nullptr, nullptr);
  app::service::OverworldRenderRequest request;
  request.scale = 0.0f;
  EXPECT_TRUE(
      absl::IsInvalidArgument(service.RenderOverworldArea(request).status()));
  request.scale = 1.0f;
  EXPECT_TRUE(absl::IsFailedPrecondition(
      service.RenderOverworldArea(request).status()));
}

}  // namespace
}  // namespace yaze::cli
