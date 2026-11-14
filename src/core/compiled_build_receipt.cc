#include "core/compiled_build_receipt.h"

#if defined(YAZE_WITH_JSON) && !defined(__EMSCRIPTEN__)

#include <exception>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

#include "absl/strings/str_cat.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"
#include "util/rom_hash.h"

namespace yaze::core {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr uint64_t kMaxReceiptBytes = 16 * 1024 * 1024;
constexpr uint64_t kMaxArtifactBytes = 1024ULL * 1024 * 1024;
constexpr uint64_t kMaxRomBytes = 64 * 1024 * 1024;

absl::Status Refused(std::string_view message) {
  return absl::FailedPreconditionError(
      absl::StrCat("Compiled build receipt refused: ", message));
}

std::string PathText(const fs::path& path) {
  const auto text = path.generic_u8string();
  return std::string(text.begin(), text.end());
}

absl::StatusOr<fs::path> Canonical(const std::string& text,
                                   std::string_view what) {
  const auto path = fs::u8path(text);
  if (text.empty() || !path.is_absolute()) {
    return Refused(absl::StrCat(what, " requires an absolute path"));
  }
  std::error_code error;
  auto canonical = fs::weakly_canonical(path, error);
  if (error) {
    return Refused(absl::StrCat("cannot resolve ", what, ": ", text));
  }
  return canonical;
}

absl::StatusOr<std::string> ReadBounded(const fs::path& path, uint64_t limit,
                                        std::string_view what) {
  std::error_code error;
  const bool regular = fs::is_regular_file(path, error);
  const auto size = regular && !error ? fs::file_size(path, error) : 0;
  if (!regular || error || size > limit) {
    return Refused(
        absl::StrCat(what, " is missing or too large: ", PathText(path)));
  }
  std::ifstream stream(path, std::ios::binary);
  std::string raw(static_cast<size_t>(size), '\0');
  stream.read(raw.data(), static_cast<std::streamsize>(raw.size()));
  char extra;
  if (stream.gcount() != static_cast<std::streamsize>(raw.size()) ||
      stream.get(extra) || stream.bad()) {
    return Refused(absl::StrCat(what, " changed while reading"));
  }
  return raw;
}

// A recorded artifact whose canonical path still holds its recorded bytes.
absl::StatusOr<fs::path> CheckArtifact(const Json& row, std::string_view what,
                                       bool require_size) {
  const auto& text = row.at("path").get_ref<const std::string&>();
  ASSIGN_OR_RETURN(auto path, Canonical(text, what));
  if (fs::u8path(text) != path) {
    return Refused(absl::StrCat(what, " path is not canonical: ", text));
  }
  std::error_code error;
  const bool regular = fs::is_regular_file(path, error);
  const auto size = regular && !error ? fs::file_size(path, error) : 0;
  if (!regular || error || size > kMaxArtifactBytes) {
    return Refused(absl::StrCat(what, " is missing: ", text));
  }
  const auto& expected = row.at("sha256").get_ref<const std::string&>();
  if (expected.size() != 64) {
    return Refused(absl::StrCat(what, " has no SHA-256"));
  }
  if (row.contains("size")) {
    if (!row.at("size").is_number_unsigned() ||
        row.at("size").get<uint64_t>() != size) {
      return Refused(absl::StrCat(what, " byte count differs: ", text));
    }
  } else if (require_size) {
    return Refused(absl::StrCat(what, " byte count is absent"));
  }
  ASSIGN_OR_RETURN(const auto actual, ComputeSourceArtifactFileSha256(path));
  if (actual != expected) {
    return Refused(absl::StrCat(what, " changed since the build: ", text));
  }
  return path;
}

}  // namespace

bool IsSuccessfulBuildReceipt(const Json& build) {
  return build.at("schema_version") == 1 &&
         (build.at("state") == "completed" ||
          build.at("state") == "completed_with_gaps") &&
         build.at("finish").at("exit_code") == 0 &&
         build.at("finish").at("required_gaps") == Json::array();
}

absl::StatusOr<CompiledBuildIdentity> VerifyProjectBuildReceipt(
    const project::YazeProject& project, const fs::path& receipt_path) {
  if (project.filepath.empty() || project.rom_filename.empty() ||
      project.code_folder.empty() || project.build_target.empty()) {
    return Refused(
        "the project must declare its ROM, code folder and build target");
  }
  try {
    CompiledBuildIdentity identity;
    ASSIGN_OR_RETURN(const auto receipt,
                     Canonical(PathText(receipt_path), "the receipt"));
    ASSIGN_OR_RETURN(const auto raw,
                     ReadBounded(receipt, kMaxReceiptBytes, "the receipt"));
    const auto build =
        Json::parse(raw, [](int depth, Json::parse_event_t, Json&) {
          if (depth > 64)
            throw std::runtime_error("receipt nesting is too deep");
          return true;
        });
    if (!build.is_object()) {
      return Refused("the receipt must be a JSON object");
    }
    if (!IsSuccessfulBuildReceipt(build)) {
      return Refused("the build is incomplete or failed");
    }

    ASSIGN_OR_RETURN(const auto root,
                     Canonical(build.at("root").get_ref<const std::string&>(),
                               "the receipt root"));
    ASSIGN_OR_RETURN(const auto code_folder,
                     Canonical(project.GetAbsolutePath(project.code_folder),
                               "the project code folder"));
    if (root != code_folder) {
      return Refused("the receipt was built from another code folder");
    }

    ASSIGN_OR_RETURN(const auto base,
                     CheckArtifact(build.at("base_rom"), "the base ROM", true));
    ASSIGN_OR_RETURN(const auto project_rom,
                     Canonical(project.GetAbsolutePath(project.rom_filename),
                               "the project ROM"));
    if (base != project_rom) {
      return Refused("the receipt's base ROM is not the project ROM");
    }
    if (!project.rom_metadata.expected_hash.empty()) {
      ASSIGN_OR_RETURN(const auto base_bytes,
                       ReadBounded(base, kMaxRomBytes, "the base ROM"));
      const auto* data = reinterpret_cast<const uint8_t*>(base_bytes.data());
      if (util::ComputeRomIdentityHashes(data, base_bytes.size())
              .Match(project.rom_metadata.expected_hash) ==
          util::RomHashMatch::kMismatch) {
        return Refused(
            "the project ROM does not match the project's expected ROM "
            "identity");
      }
    }

    const auto& artifacts = build.at("artifacts");
    ASSIGN_OR_RETURN(
        const auto output,
        CheckArtifact(artifacts.at("output_rom"), "the output ROM", true));
    ASSIGN_OR_RETURN(const auto target,
                     Canonical(project.GetAbsolutePath(project.build_target),
                               "the project build target"));
    if (output != target) {
      return Refused("the receipt's output is not the project build target");
    }
    std::error_code alias_error;
    if (output == base || fs::equivalent(output, base, alias_error)) {
      return Refused("the output ROM aliases the base ROM");
    }
    if (artifacts.contains("manifest")) {
      ASSIGN_OR_RETURN(const auto manifest,
                       CheckArtifact(artifacts.at("manifest"),
                                     "the output manifest", false));
      identity.manifest_path = PathText(manifest);
      identity.manifest_sha256 =
          artifacts.at("manifest").at("sha256").get<std::string>();
    }

    identity.receipt_path = PathText(receipt);
    identity.receipt_sha256 = ComputeSourceArtifactSha256(raw);
    identity.state = build.at("state").get<std::string>();
    identity.base_rom_path = PathText(base);
    identity.base_rom_sha256 =
        build.at("base_rom").at("sha256").get<std::string>();
    identity.output_rom_path = PathText(output);
    identity.output_rom_sha256 =
        artifacts.at("output_rom").at("sha256").get<std::string>();
    identity.output_rom_size =
        artifacts.at("output_rom").at("size").get<uint64_t>();
    return identity;
  } catch (const std::exception& error) {
    return Refused(absl::StrCat("malformed receipt: ", error.what()));
  }
}

absl::Status CheckCompiledBuildBytes(const CompiledBuildIdentity& build,
                                     const uint8_t* data, size_t size) {
  if (data == nullptr || size != build.output_rom_size ||
      ComputeSourceArtifactSha256(
          std::string_view(reinterpret_cast<const char*>(data), size)) !=
          build.output_rom_sha256) {
    return Refused("the loaded ROM is not the receipt's output");
  }
  return absl::OkStatus();
}

}  // namespace yaze::core

#else

namespace yaze::core {

bool IsSuccessfulBuildReceipt(const nlohmann::json&) {
  return false;
}

absl::StatusOr<CompiledBuildIdentity> VerifyProjectBuildReceipt(
    const project::YazeProject&, const std::filesystem::path&) {
  return absl::UnimplementedError(
      "Compiled build receipts require desktop JSON support");
}

absl::Status CheckCompiledBuildBytes(const CompiledBuildIdentity&,
                                     const uint8_t*, size_t) {
  return absl::UnimplementedError(
      "Compiled build receipts require desktop JSON support");
}

}  // namespace yaze::core

#endif
