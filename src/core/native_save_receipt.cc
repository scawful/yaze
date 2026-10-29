#include "core/native_save_receipt.h"

#include "absl/status/status.h"

#if defined(YAZE_WITH_JSON)

#include <algorithm>
#include <array>
#include <exception>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"
#include "util/platform_paths.h"

namespace yaze::core {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr char kKind[] = "yaze.native_save.v1";
constexpr char kContextKind[] = "yaze.native_save_context.v1";
constexpr uint64_t kMaxRomBytes = 64 * 1024 * 1024;
constexpr uint64_t kMaxReceiptBytes = 1024 * 1024;
constexpr size_t kMaxArtifacts = 256;

bool IsIdentifier(std::string_view value, bool role = false) {
  if (value.empty() || value.size() > 128 ||
      (role && (value.front() < 'a' || value.front() > 'z'))) {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [role](char c) {
    return (c >= 'a' && c <= 'z') || (!role && c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '_' || c == ':' ||
           c == '-';
  });
}

bool IsSha256(std::string_view value) {
  return value.size() == 64 &&
         std::all_of(value.begin(), value.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

bool IsOperationId(std::string_view value) {
  return !value.empty() && value.size() <= 128 &&
         std::all_of(value.begin(), value.end(),
                     [](char c) { return c >= 0x20 && c <= 0x7E; });
}

std::string PathText(const fs::path& path) {
  const auto bytes = path.generic_u8string();
  return std::string(bytes.begin(), bytes.end());
}

bool IsAbsolutePath(const fs::path& path) {
  return !path.empty() && path.is_absolute() &&
         path.string().find('\0') == std::string::npos;
}

bool AreBindingErrorsValid(const std::vector<std::string>& errors) {
  return errors.size() <= kMaxArtifacts &&
         std::all_of(errors.begin(), errors.end(), [](const auto& error) {
           return !error.empty() && error.size() <= 1024 &&
                  error.find('\0') == std::string::npos;
         });
}

absl::StatusOr<fs::path> ResolveRegularFile(const fs::path& path) {
  std::error_code error;
  const auto status = fs::symlink_status(path, error);
  if (error || !fs::is_regular_file(status)) {
    return absl::FailedPreconditionError(
        absl::StrCat("Native save input must be a regular, non-symlink file: ",
                     path.string()));
  }
  auto resolved = fs::canonical(path, error);
  if (error) {
    return absl::FailedPreconditionError(
        absl::StrCat("Cannot resolve native save input: ", path.string()));
  }
  return resolved;
}

absl::StatusOr<uint64_t> FileSize(const fs::path& path) {
  std::error_code error;
  const auto size = fs::file_size(path, error);
  if (error || size > std::numeric_limits<uint64_t>::max()) {
    return absl::FailedPreconditionError(
        absl::StrCat("Cannot measure native save input: ", path.string()));
  }
  return static_cast<uint64_t>(size);
}

absl::StatusOr<std::string> ReadBoundedFile(const fs::path& path,
                                            uint64_t maximum) {
  ASSIGN_OR_RETURN(const auto size, FileSize(path));
  if (size > maximum) {
    return absl::InvalidArgumentError("Native save input exceeds size limit");
  }
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return absl::FailedPreconditionError("Cannot read native save input");
  }
  std::string content;
  std::array<char, 64 * 1024> buffer;
  while (input) {
    input.read(buffer.data(), buffer.size());
    const auto count = static_cast<uint64_t>(input.gcount());
    if (count > maximum - content.size()) {
      return absl::InvalidArgumentError("Native save input exceeds size limit");
    }
    content.append(buffer.data(), static_cast<size_t>(count));
  }
  if (!input.eof() || input.bad()) {
    return absl::DataLossError("Failed while reading native save input");
  }
  return content;
}

absl::StatusOr<std::optional<std::string>> ReadOptionalOutput(
    const fs::path& path) {
  std::error_code error;
  const auto status = fs::symlink_status(path, error);
  if (error == std::errc::no_such_file_or_directory ||
      (!error && status.type() == fs::file_type::not_found)) {
    return std::nullopt;
  }
  ASSIGN_OR_RETURN(const auto resolved, ResolveRegularFile(path));
  ASSIGN_OR_RETURN(auto content, ReadBoundedFile(resolved, kMaxReceiptBytes));
  return std::optional<std::string>(std::move(content));
}

bool IsProducerArtifact(const Json& value) {
  return value.is_object() && value.size() == 4 &&
         value.value("role", Json()) == "producer" && value.contains("path") &&
         value["path"].is_string() &&
         IsAbsolutePath(fs::u8path(value["path"].get<std::string>())) &&
         value.contains("sha256") && value["sha256"].is_string() &&
         IsSha256(value["sha256"].get_ref<const std::string&>()) &&
         value.contains("bytes") && value["bytes"].is_number_unsigned();
}

absl::Status ValidateContext(const NativeSaveContext& context) {
  ASSIGN_OR_RETURN(
      const auto expected,
      GetNativeSaveContext(context.editing_source_path, context.project_path));
  if (context.path != expected.path ||
      context.editing_source_path != expected.editing_source_path ||
      context.project_path != expected.project_path) {
    return absl::InvalidArgumentError("Native save context is not current");
  }
  return absl::OkStatus();
}

absl::Status ValidatePreviousContext(const Json& value,
                                     const NativeSaveContext& context) {
  if (!value.is_object() || value.size() != 7 ||
      value.value("kind", Json()) != kContextKind ||
      value.value("editing_source_path", Json()) !=
          context.editing_source_path ||
      !value.contains("project_path") || !value["project_path"].is_string() ||
      !value.contains("state") || !value["state"].is_string() ||
      !value.contains("rom_path") || !value["rom_path"].is_string() ||
      !IsAbsolutePath(fs::u8path(value["rom_path"].get<std::string>())) ||
      !value.contains("producer") || !IsProducerArtifact(value["producer"]) ||
      !value.contains("native_commit")) {
    return absl::FailedPreconditionError("Invalid prior native save context");
  }
  const auto project_path = value["project_path"].get<std::string>();
  if (!project_path.empty() && !IsAbsolutePath(fs::u8path(project_path))) {
    return absl::FailedPreconditionError("Invalid native save context project");
  }
  if (value["state"] == "committed") {
    const auto& commit = value["native_commit"];
    if (!commit.is_object() || commit.size() != 3 ||
        !commit.contains("receipt_path") ||
        !commit["receipt_path"].is_string() ||
        !IsAbsolutePath(
            fs::u8path(commit["receipt_path"].get<std::string>())) ||
        !commit.contains("generation") ||
        !commit["generation"].is_number_unsigned() ||
        commit["generation"].get<uint64_t>() == 0 ||
        !commit.contains("operation_id") ||
        !commit["operation_id"].is_string() ||
        !IsOperationId(commit["operation_id"].get_ref<const std::string&>())) {
      return absl::FailedPreconditionError(
          "Invalid committed native save context");
    }
  } else if (value["state"] != "rebound" || !value["native_commit"].is_null()) {
    return absl::FailedPreconditionError("Invalid retired native save context");
  }
  return absl::OkStatus();
}

absl::Status ValidatePreviousReceipt(const Json& receipt,
                                     const fs::path& receipt_path) {
  if (!receipt.is_object() || receipt.size() != 8 ||
      receipt.value("kind", Json()) != kKind ||
      !receipt.contains("generation") ||
      !receipt["generation"].is_number_unsigned() ||
      receipt["generation"].get<uint64_t>() == 0 ||
      !receipt.contains("operation_id") ||
      !receipt["operation_id"].is_string() ||
      !IsOperationId(receipt["operation_id"].get_ref<const std::string&>()) ||
      (receipt.value("operation", Json()) != "save" &&
       receipt.value("operation", Json()) != "save_as") ||
      receipt.value("receipt_path", Json()) != PathText(receipt_path) ||
      !receipt.contains("bindings_complete") ||
      !receipt["bindings_complete"].is_boolean() ||
      !receipt.contains("binding_errors") ||
      !receipt["binding_errors"].is_array() || !receipt.contains("artifacts") ||
      !receipt["artifacts"].is_array() ||
      receipt["artifacts"].size() > kMaxArtifacts) {
    return absl::FailedPreconditionError("Invalid prior native save receipt");
  }
  std::vector<std::string> errors;
  for (const auto& error : receipt["binding_errors"]) {
    if (!error.is_string()) {
      return absl::FailedPreconditionError("Invalid prior native save errors");
    }
    errors.push_back(error.get<std::string>());
  }
  if (!AreBindingErrorsValid(errors) ||
      receipt["bindings_complete"] != errors.empty()) {
    return absl::FailedPreconditionError("Invalid prior native save errors");
  }
  std::set<std::string> roles;
  std::map<std::string, const Json*> paths;
  for (const auto& artifact : receipt["artifacts"]) {
    if (!artifact.is_object() || artifact.size() != 4 ||
        !artifact.contains("role") || !artifact["role"].is_string() ||
        !artifact.contains("path") || !artifact["path"].is_string() ||
        !artifact.contains("sha256") || !artifact["sha256"].is_string() ||
        !artifact.contains("bytes") ||
        !artifact["bytes"].is_number_unsigned()) {
      return absl::FailedPreconditionError(
          "Invalid prior native save artifact");
    }
    const auto& role = artifact["role"].get_ref<const std::string&>();
    const fs::path path = fs::u8path(artifact["path"].get<std::string>());
    const auto& hash = artifact["sha256"].get_ref<const std::string&>();
    const auto [existing, inserted] =
        paths.emplace(absl::AsciiStrToLower(path.generic_string()), &artifact);
    const bool conflicting_path =
        !inserted && ((*existing->second)["path"] != artifact["path"] ||
                      (*existing->second)["sha256"] != artifact["sha256"] ||
                      (*existing->second)["bytes"] != artifact["bytes"]);
    if (!IsIdentifier(role, true) || !roles.insert(role).second ||
        !IsAbsolutePath(path) || path != path.lexically_normal() ||
        conflicting_path || !IsSha256(hash)) {
      return absl::FailedPreconditionError("Invalid prior native save binding");
    }
  }
  if (!roles.contains("rom") || !roles.contains("producer")) {
    return absl::FailedPreconditionError("Prior native save roles are missing");
  }
  return absl::OkStatus();
}

struct ArtifactBinding {
  NativeSaveArtifact artifact;
  fs::path resolved;
  uint64_t bytes;
};

absl::Status VerifyBinding(const ArtifactBinding& binding) {
  ASSIGN_OR_RETURN(const auto resolved,
                   ResolveRegularFile(binding.artifact.path));
  if (resolved != binding.resolved) {
    return absl::AbortedError("Native save input path changed");
  }
  ASSIGN_OR_RETURN(const auto size, FileSize(resolved));
  ASSIGN_OR_RETURN(const auto hash, ComputeSourceArtifactFileSha256(resolved));
  ASSIGN_OR_RETURN(const auto final_resolved,
                   ResolveRegularFile(binding.artifact.path));
  ASSIGN_OR_RETURN(const auto final_size, FileSize(final_resolved));
  if (final_resolved != resolved || size != binding.bytes ||
      final_size != binding.bytes || hash != binding.artifact.expected_sha256) {
    return absl::AbortedError("Native save artifact changed after save");
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<NativeSaveContext> GetNativeSaveContext(
    const std::string& editing_source_path, const std::string& project_path) {
  if (!IsAbsolutePath(fs::u8path(editing_source_path)) ||
      (!project_path.empty() && !IsAbsolutePath(fs::u8path(project_path)))) {
    return absl::InvalidArgumentError(
        "Native save editing-source and project paths must be absolute");
  }
  std::error_code error;
  const auto origin =
      fs::weakly_canonical(fs::u8path(editing_source_path), error);
  if (error) {
    return absl::FailedPreconditionError(
        "Cannot resolve native editing source");
  }
  const auto origin_text = PathText(origin);
  ASSIGN_OR_RETURN(const auto app_data,
                   util::PlatformPaths::GetAppDataDirectory());
  const auto directory = fs::absolute(app_data, error) / "native-saves";
  if (error) {
    return absl::FailedPreconditionError(
        "Cannot resolve native save directory");
  }
  fs::create_directories(directory, error);
  if (error || !fs::is_directory(fs::symlink_status(directory, error))) {
    return absl::FailedPreconditionError("Cannot create native save directory");
  }
  const auto path = fs::canonical(directory, error) /
                    (ComputeSourceArtifactSha256(origin_text) + ".json");
  if (error) {
    return absl::FailedPreconditionError("Cannot resolve native save context");
  }
  std::string normalized_project;
  if (!project_path.empty()) {
    normalized_project =
        PathText(fs::weakly_canonical(fs::u8path(project_path), error));
    if (error) {
      return absl::FailedPreconditionError(
          "Cannot resolve native save project");
    }
  }
  return NativeSaveContext{path, origin_text, std::move(normalized_project)};
}

absl::Status RetireNativeSaveContext(const NativeSaveContext& context,
                                     const std::string& state) {
  if (state != "rebound" && state != "session_changed") {
    return absl::InvalidArgumentError("Invalid native save retirement state");
  }
  RETURN_IF_ERROR(ValidateContext(context));
  ASSIGN_OR_RETURN(const auto existing, ReadOptionalOutput(context.path));
  if (!existing.has_value()) {
    return absl::OkStatus();
  }

  ASSIGN_OR_RETURN(auto lock,
                   AcquireSourceArtifactPublicationLock({context.path}));
  ASSIGN_OR_RETURN(const auto before, ReadOptionalOutput(context.path));
  if (!before.has_value()) {
    return absl::AbortedError("Native save context disappeared");
  }
  RETURN_IF_ERROR(
      ValidatePreviousContext(Json::parse(*before, nullptr, false), context));
  const auto previous = Json::parse(*before);
  const Json retired = {{"kind", kContextKind},
                        {"editing_source_path", context.editing_source_path},
                        {"state", "rebound"},
                        {"project_path", context.project_path},
                        {"rom_path", previous["rom_path"]},
                        {"producer", previous["producer"]},
                        {"native_commit", nullptr}};
  std::string text;
  try {
    text = retired.dump(2) + "\n";
  } catch (const Json::exception&) {
    return absl::InvalidArgumentError(
        "Native save context must be valid UTF-8");
  }
  return PublishSourceArtifacts(
      *lock, {{context.path, before, text}},
      ComputeSourceArtifactSha256(*before), [&]() -> absl::Status {
        ASSIGN_OR_RETURN(const auto resolved, ResolveRegularFile(context.path));
        if (resolved != context.path) {
          return absl::AbortedError("Native save context path changed");
        }
        return absl::OkStatus();
      });
}

absl::StatusOr<NativeSaveReceipt> VerifyCurrentNativeSaveReceipt(
    const fs::path& requested_receipt,
    const std::optional<NativeSaveContext>& context,
    const std::vector<NativeSaveArtifact>& required_artifacts,
    NativeSaveReceiptVerification policy) {
  if (context.has_value() &&
      policy == NativeSaveReceiptVerification::kPersistedDataAdmission) {
    return absl::InvalidArgumentError(
        "Current native save context requires current-artifact verification");
  }
  ASSIGN_OR_RETURN(const auto receipt_path,
                   ResolveRegularFile(requested_receipt));
  ASSIGN_OR_RETURN(const auto receipt_text,
                   ReadBoundedFile(receipt_path, kMaxReceiptBytes));
  std::optional<std::string> context_text;
  if (context.has_value()) {
    RETURN_IF_ERROR(ValidateContext(*context));
    ASSIGN_OR_RETURN(const auto context_path,
                     ResolveRegularFile(context->path));
    ASSIGN_OR_RETURN(context_text,
                     ReadBoundedFile(context_path, kMaxReceiptBytes));
  }
  try {
    const auto receipt = Json::parse(receipt_text);
    RETURN_IF_ERROR(ValidatePreviousReceipt(receipt, receipt_path));
    if (receipt["bindings_complete"] != true) {
      return absl::FailedPreconditionError(
          "Native save receipt does not have complete bindings");
    }
    std::map<std::string, const Json*> roles;
    std::vector<ArtifactBinding> bindings;
    for (const auto& artifact : receipt["artifacts"]) {
      const auto role = artifact["role"].get<std::string>();
      roles.emplace(role, &artifact);
      if (role == "producer" &&
          policy == NativeSaveReceiptVerification::kPersistedDataAdmission) {
        continue;
      }
      const auto path = fs::u8path(artifact["path"].get<std::string>());
      ASSIGN_OR_RETURN(const auto resolved, ResolveRegularFile(path));
      if (path != resolved) {
        return absl::FailedPreconditionError(
            "Native save artifact path is no longer canonical");
      }
      ArtifactBinding binding{{artifact["role"].get<std::string>(), path,
                               artifact["sha256"].get<std::string>()},
                              resolved,
                              artifact["bytes"].get<uint64_t>()};
      RETURN_IF_ERROR(VerifyBinding(binding));
      bindings.push_back(std::move(binding));
    }
    if (context.has_value()) {
      const auto current = Json::parse(*context_text);
      RETURN_IF_ERROR(ValidatePreviousContext(current, *context));
      if (current["state"] != "committed" ||
          current["project_path"] != context->project_path ||
          current["native_commit"]["receipt_path"] != PathText(receipt_path) ||
          current["native_commit"]["generation"] != receipt["generation"] ||
          current["native_commit"]["operation_id"] != receipt["operation_id"] ||
          current["rom_path"] != (*roles.at("rom"))["path"] ||
          current["producer"] != *roles.at("producer")) {
        return absl::FailedPreconditionError(
            "Native save receipt is not the current committed generation");
      }
    }
    std::set<std::string> required_roles;
    for (const auto& required : required_artifacts) {
      if (!IsIdentifier(required.role, true) ||
          !required_roles.insert(required.role).second ||
          !IsSha256(required.expected_sha256)) {
        return absl::InvalidArgumentError(
            "Invalid required native save artifact");
      }
      ASSIGN_OR_RETURN(const auto path, ResolveRegularFile(required.path));
      const auto found = roles.find(required.role);
      if (found == roles.end() || (*found->second)["path"] != PathText(path) ||
          (*found->second)["sha256"] != required.expected_sha256) {
        return absl::FailedPreconditionError(
            "Required native save artifact binding differs: " + required.role);
      }
    }
    for (const auto& binding : bindings) {
      RETURN_IF_ERROR(VerifyBinding(binding));
    }
    ASSIGN_OR_RETURN(const auto receipt_after,
                     ReadBoundedFile(receipt_path, kMaxReceiptBytes));
    ASSIGN_OR_RETURN(const auto final_receipt_path,
                     ResolveRegularFile(requested_receipt));
    if (receipt_after != receipt_text || final_receipt_path != receipt_path) {
      return absl::AbortedError("Native save admission changed while reading");
    }
    if (context.has_value()) {
      ASSIGN_OR_RETURN(const auto context_after,
                       ReadBoundedFile(context->path, kMaxReceiptBytes));
      ASSIGN_OR_RETURN(const auto final_context_path,
                       ResolveRegularFile(context->path));
      if (context_after != *context_text ||
          final_context_path != context->path) {
        return absl::AbortedError("Native save context changed while reading");
      }
    }
    return NativeSaveReceipt{receipt["generation"].get<uint64_t>(),
                             receipt_text};
  } catch (const std::exception& error) {
    return absl::FailedPreconditionError(
        std::string("Invalid native save admission: ") + error.what());
  }
}

absl::StatusOr<NativeSaveReceipt> PublishNativeSaveReceipt(
    const NativeSaveReceiptRequest& request) {
  if (!IsAbsolutePath(request.receipt_path) ||
      !IsOperationId(request.operation_id) ||
      (request.operation != "save" && request.operation != "save_as") ||
      request.artifacts.size() > kMaxArtifacts ||
      !AreBindingErrorsValid(request.binding_errors)) {
    return absl::InvalidArgumentError("Invalid native save receipt request");
  }
  std::set<std::string> roles;
  std::vector<fs::path> inputs;
  std::map<fs::path, std::string> input_hashes;
  std::vector<ArtifactBinding> bindings;
  for (const auto& artifact : request.artifacts) {
    if (!IsIdentifier(artifact.role, true) ||
        !roles.insert(artifact.role).second || !IsAbsolutePath(artifact.path) ||
        !IsSha256(artifact.expected_sha256)) {
      return absl::InvalidArgumentError("Invalid native save artifact binding");
    }
    ASSIGN_OR_RETURN(auto resolved, ResolveRegularFile(artifact.path));
    ASSIGN_OR_RETURN(const auto size, FileSize(resolved));
    if (artifact.role == "rom" && size > kMaxRomBytes) {
      return absl::InvalidArgumentError("Native save ROM exceeds size limit");
    }
    const auto [existing, inserted] =
        input_hashes.emplace(resolved, artifact.expected_sha256);
    if (!inserted && existing->second != artifact.expected_sha256) {
      return absl::InvalidArgumentError("Native save input hashes conflict");
    }
    if (inserted) {
      inputs.push_back(resolved);
    }
    bindings.push_back({artifact, std::move(resolved), size});
  }
  if (!roles.contains("rom") || !roles.contains("producer")) {
    return absl::InvalidArgumentError("Native save requires rom and producer");
  }
  std::vector<fs::path> outputs{request.receipt_path};
  if (request.context.has_value()) {
    RETURN_IF_ERROR(ValidateContext(*request.context));
    outputs.push_back(request.context->path);
  }

  std::error_code error;
  const auto receipt_path = fs::weakly_canonical(request.receipt_path, error);
  if (error) {
    return absl::FailedPreconditionError("Cannot resolve native save receipt");
  }
  ASSIGN_OR_RETURN(
      auto lock,
      AcquireSourceArtifactPublicationLock(
          outputs, {"native save receipt", "native save receipt"}, inputs));
  const auto receipt_after_lock =
      fs::weakly_canonical(request.receipt_path, error);
  if (error || receipt_after_lock != receipt_path) {
    return absl::AbortedError("Native save receipt path changed");
  }
  for (const auto& binding : bindings) {
    RETURN_IF_ERROR(VerifyBinding(binding));
  }
  std::sort(bindings.begin(), bindings.end(),
            [](const auto& left, const auto& right) {
              return left.artifact.role < right.artifact.role;
            });
  Json artifacts = Json::array();
  const ArtifactBinding* rom = nullptr;
  Json producer;
  for (const auto& binding : bindings) {
    artifacts.push_back({{"role", binding.artifact.role},
                         {"path", PathText(binding.resolved)},
                         {"sha256", binding.artifact.expected_sha256},
                         {"bytes", binding.bytes}});
    if (binding.artifact.role == "rom") {
      rom = &binding;
    } else if (binding.artifact.role == "producer") {
      producer = artifacts.back();
    }
  }
  ASSIGN_OR_RETURN(auto rom_bytes,
                   ReadBoundedFile(rom->resolved, kMaxRomBytes));
  if (ComputeSourceArtifactSha256(rom_bytes) != rom->artifact.expected_sha256) {
    return absl::AbortedError("Native save ROM changed after save");
  }

  std::optional<std::string> before;
  std::optional<std::string> context_before;
  std::optional<Json> context_previous;
  if (request.context.has_value()) {
    ASSIGN_OR_RETURN(context_before, ReadOptionalOutput(request.context->path));
    if (context_before.has_value()) {
      context_previous = Json::parse(*context_before, nullptr, false);
      RETURN_IF_ERROR(
          ValidatePreviousContext(*context_previous, *request.context));
    }
  }
  uint64_t generation = 1;
  Json receipt = {{"kind", kKind},
                  {"generation", generation},
                  {"operation_id", request.operation_id},
                  {"operation", request.operation},
                  {"receipt_path", PathText(receipt_path)},
                  {"bindings_complete", request.binding_errors.empty()},
                  {"binding_errors", request.binding_errors},
                  {"artifacts", std::move(artifacts)}};
  bool replay = false;
  const bool exists = fs::exists(receipt_path, error);
  if (error) {
    return absl::FailedPreconditionError("Cannot inspect native save receipt");
  }
  if (exists) {
    ASSIGN_OR_RETURN(auto resolved, ResolveRegularFile(receipt_path));
    ASSIGN_OR_RETURN(before, ReadBoundedFile(resolved, kMaxReceiptBytes));
    const auto previous = Json::parse(*before, nullptr, false);
    RETURN_IF_ERROR(ValidatePreviousReceipt(previous, receipt_path));
    generation = previous["generation"].get<uint64_t>();
    if (previous["operation_id"] == request.operation_id) {
      replay = true;
      if (previous["operation"] != request.operation ||
          previous["artifacts"] != receipt["artifacts"] ||
          previous["binding_errors"] != receipt["binding_errors"]) {
        return absl::AbortedError("Native save operation_id was reused");
      }
    } else if (generation == std::numeric_limits<uint64_t>::max()) {
      return absl::OutOfRangeError("Native save generation overflow");
    } else {
      ++generation;
    }
    receipt["generation"] = generation;
  }
  std::string text;
  try {
    text = receipt.dump(2) + "\n";
  } catch (const Json::exception&) {
    return absl::InvalidArgumentError("Native save paths must be valid UTF-8");
  }
  if (text.size() > kMaxReceiptBytes) {
    return absl::InvalidArgumentError("Native save receipt exceeds size limit");
  }
  // Idempotent retries preserve the original receipt bytes, including spacing.
  if (before.has_value() && Json::parse(*before) == receipt) {
    text = *before;
  }
  std::optional<std::string> context_text;
  if (request.context.has_value()) {
    const auto& context = *request.context;
    const Json committed = {
        {"kind", kContextKind},
        {"editing_source_path", context.editing_source_path},
        {"state", "committed"},
        {"project_path", context.project_path},
        {"rom_path", PathText(rom->resolved)},
        {"producer", producer},
        {"native_commit",
         {{"receipt_path", PathText(receipt_path)},
          {"generation", generation},
          {"operation_id", request.operation_id}}}};
    if (replay &&
        (!context_previous.has_value() || *context_previous != committed)) {
      return absl::AbortedError(
          "Native save retry cannot reactivate a retired context");
    }
    try {
      context_text = committed.dump(2) + "\n";
    } catch (const Json::exception&) {
      return absl::InvalidArgumentError(
          "Native save context must be valid UTF-8");
    }
    if (replay) {
      context_text = context_before;
    }
  }
  std::vector<SourceArtifactUpdate> updates;
  updates.push_back({rom->resolved, rom_bytes, std::move(rom_bytes)});
  updates.push_back({receipt_path, std::move(before), text});
  if (request.context.has_value()) {
    updates.push_back({request.context->path, context_before, *context_text});
  }
  RETURN_IF_ERROR(PublishSourceArtifacts(
      *lock, std::move(updates), rom->artifact.expected_sha256, [&]() {
        for (const auto& binding : bindings) {
          RETURN_IF_ERROR(VerifyBinding(binding));
        }
        ASSIGN_OR_RETURN(const auto resolved_receipt,
                         ResolveRegularFile(request.receipt_path));
        if (resolved_receipt != receipt_path) {
          return absl::AbortedError("Native save receipt path changed");
        }
        ASSIGN_OR_RETURN(const auto readback,
                         ReadBoundedFile(receipt_path, kMaxReceiptBytes));
        if (readback != text) {
          return absl::DataLossError("Native save receipt readback changed");
        }
        if (request.context.has_value()) {
          ASSIGN_OR_RETURN(const auto resolved_context,
                           ResolveRegularFile(request.context->path));
          if (resolved_context != request.context->path) {
            return absl::AbortedError("Native save context path changed");
          }
          ASSIGN_OR_RETURN(const auto context_readback,
                           ReadBoundedFile(resolved_context, kMaxReceiptBytes));
          if (context_readback != *context_text) {
            return absl::DataLossError("Native save context readback changed");
          }
        }
        return absl::OkStatus();
      }));
  return NativeSaveReceipt{generation, std::move(text)};
}

}  // namespace yaze::core

#else

namespace yaze::core {

absl::StatusOr<NativeSaveContext> GetNativeSaveContext(const std::string&,
                                                       const std::string&) {
  return absl::UnimplementedError("Native save contexts require JSON support");
}

absl::Status RetireNativeSaveContext(const NativeSaveContext&,
                                     const std::string&) {
  return absl::UnimplementedError("Native save contexts require JSON support");
}

absl::StatusOr<NativeSaveReceipt> PublishNativeSaveReceipt(
    const NativeSaveReceiptRequest&) {
  return absl::UnimplementedError("Native save receipts require JSON support");
}

absl::StatusOr<NativeSaveReceipt> VerifyCurrentNativeSaveReceipt(
    const std::filesystem::path&, const std::optional<NativeSaveContext>&,
    const std::vector<NativeSaveArtifact>&, NativeSaveReceiptVerification) {
  return absl::UnimplementedError("Native save receipts require JSON support");
}

}  // namespace yaze::core

#endif  // defined(YAZE_WITH_JSON)
