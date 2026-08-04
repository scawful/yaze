#include "app/editor/message/message_source_sync.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/message/message_data.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "rom/snes.h"
#include "util/macro.h"

namespace yaze::editor {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

struct ExpandedSourceMessage {
  std::string text;
  std::vector<uint8_t> bytes;
};

using ExpandedSourceBank = std::map<int, ExpandedSourceMessage>;

const core::SourceArtifactPublisherLabels kMessageSourcePublisherLabels{
    .subject = "message source",
    .published_file = "published message file",
};

absl::StatusOr<std::string> ReadTextFile(const fs::path& path,
                                         absl::string_view label) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open %s: %s", label, path.string()));
  }
  std::string content{std::istreambuf_iterator<char>(input),
                      std::istreambuf_iterator<char>()};
  if (!input.good() && !input.eof()) {
    return absl::DataLossError(
        absl::StrFormat("Failed while reading %s: %s", label, path.string()));
  }
  return content;
}

bool ReadBoundedNonnegativeInteger(const Json& value, uint64_t maximum,
                                   uint64_t* parsed) {
  if (value.is_number_unsigned()) {
    const uint64_t candidate = value.get<uint64_t>();
    if (candidate > maximum) {
      return false;
    }
    *parsed = candidate;
    return true;
  }
  if (!value.is_number_integer()) {
    return false;
  }
  const int64_t candidate = value.get<int64_t>();
  if (candidate < 0 || static_cast<uint64_t>(candidate) > maximum) {
    return false;
  }
  *parsed = static_cast<uint64_t>(candidate);
  return true;
}

absl::StatusOr<Json> ParseBundleDocument(std::string_view content,
                                         absl::string_view label) {
  Json document;
  try {
    document = Json::parse(content);
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s is not valid JSON: %s", label, error.what()));
  }
  if (!document.is_object()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s must be a JSON object", label));
  }
  if (!document.contains("format") || !document["format"].is_string() ||
      document["format"].get<std::string>() != "yaze-message-bundle") {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s format must be 'yaze-message-bundle'", label));
  }
  uint64_t version = 0;
  if (!document.contains("version") ||
      !ReadBoundedNonnegativeInteger(
          document["version"],
          static_cast<uint64_t>(std::numeric_limits<int>::max()), &version) ||
      version != static_cast<uint64_t>(kMessageBundleVersion)) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%s version must be integer %d", label, kMessageBundleVersion));
  }
  if (!document.contains("messages") || !document["messages"].is_array()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s must contain a messages array", label));
  }
  if (document.contains("counts")) {
    uint64_t expanded_count = 0;
    uint64_t vanilla_count = 0;
    if (!document["counts"].is_object() ||
        !document["counts"].contains("expanded") ||
        !ReadBoundedNonnegativeInteger(
            document["counts"]["expanded"],
            static_cast<uint64_t>(std::numeric_limits<int>::max()),
            &expanded_count) ||
        !document["counts"].contains("vanilla") ||
        !ReadBoundedNonnegativeInteger(
            document["counts"]["vanilla"],
            static_cast<uint64_t>(std::numeric_limits<int>::max()),
            &vanilla_count)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s counts must use bounded non-negative integers", label));
    }
  }
  return document;
}

absl::StatusOr<std::string> SelectEntryText(const Json& entry, int id,
                                            absl::string_view label) {
  // `text` is the editable canonical field. Rich exports may retain stale
  // raw/parsed diagnostics beside a deliberately changed text value.
  for (absl::string_view key : {"text", "raw", "parsed"}) {
    const std::string key_string(key);
    if (!entry.contains(key_string)) {
      continue;
    }
    if (!entry[key_string].is_string()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s expanded message %d field '%s' must be a string",
                          label, id, key));
    }
    return entry[key_string].get<std::string>();
  }
  return absl::InvalidArgumentError(absl::StrFormat(
      "%s expanded message %d has no raw, text, or parsed string", label, id));
}

std::string NormalizeLegacyDictionaryTokens(std::string text) {
  size_t search_from = 0;
  while (true) {
    const size_t token_start = text.find("[D:", search_from);
    if (token_start == std::string::npos) {
      break;
    }
    size_t value_start = token_start + 3;
    if (value_start < text.size() && text[value_start] == '$') {
      ++value_start;
    }
    const size_t token_end = text.find(']', value_start);
    if (token_end == std::string::npos) {
      break;
    }
    const size_t digit_count = token_end - value_start;
    const bool valid_digits =
        digit_count >= 1 && digit_count <= 2 &&
        std::all_of(text.begin() + static_cast<std::ptrdiff_t>(value_start),
                    text.begin() + static_cast<std::ptrdiff_t>(token_end),
                    [](unsigned char c) { return std::isxdigit(c) != 0; });
    if (!valid_digits) {
      search_from = token_end + 1;
      continue;
    }
    const int dictionary_id =
        std::stoi(text.substr(value_start, digit_count), nullptr, 16);
    const std::string normalized = absl::StrFormat("[D:%02X]", dictionary_id);
    text.replace(token_start, token_end - token_start + 1, normalized);
    search_from = token_start + normalized.size();
  }
  return text;
}

absl::StatusOr<ExpandedSourceBank> ParseExpandedBank(const Json& document,
                                                     int expected_count,
                                                     bool require_complete,
                                                     absl::string_view label) {
  ExpandedSourceBank bank;
  for (const Json& entry : document["messages"]) {
    if (!entry.is_object()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s message entry must be an object", label));
    }
    uint64_t id_value = 0;
    if (!entry.contains("id") ||
        !ReadBoundedNonnegativeInteger(
            entry["id"], static_cast<uint64_t>(expected_count - 1),
            &id_value)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s message entry ID must be an integer in [0, %d)",
                          label, expected_count));
    }
    const int id = static_cast<int>(id_value);
    if (!entry.contains("bank") || !entry["bank"].is_string() ||
        entry["bank"].get<std::string>() != "expanded") {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s message %d must declare bank 'expanded'", label, id));
    }
    if (bank.contains(id)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s contains duplicate expanded message ID %d", label, id));
    }

    std::string text;
    ASSIGN_OR_RETURN(text, SelectEntryText(entry, id, label));
    text = NormalizeLegacyDictionaryTokens(std::move(text));
    if (absl::StrContains(text, "[BANK]")) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s expanded message %d contains [BANK], which is only valid in "
          "the vanilla message stream",
          label, id));
    }
    MessageParseResult parsed;
    try {
      parsed = ParseMessageToDataWithDiagnostics(text);
    } catch (const std::exception& error) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s expanded message %d cannot be parsed safely: %s",
                          label, id, error.what()));
    } catch (...) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s expanded message %d cannot be parsed safely", label, id));
    }
    if (!parsed.ok()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s expanded message %d is invalid: %s", label, id,
                          parsed.errors.front()));
    }
    if (!parsed.warnings.empty()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s expanded message %d is not source-stable: %s",
                          label, id, parsed.warnings.front()));
    }
    bank.emplace(id, ExpandedSourceMessage{
                         .text = std::move(text),
                         .bytes = std::move(parsed.bytes),
                     });
  }

  if (bank.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s has no expanded messages", label));
  }
  if (require_complete) {
    if (bank.size() != static_cast<size_t>(expected_count)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "%s must contain the complete expanded bank: expected %d entries, "
          "found %zu",
          label, expected_count, bank.size()));
    }
    for (int id = 0; id < expected_count; ++id) {
      if (!bank.contains(id)) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "%s is missing bank-local expanded message ID %d", label, id));
      }
    }
    uint64_t expanded_count = 0;
    uint64_t vanilla_count = 0;
    if (!document.contains("counts") || !document["counts"].is_object() ||
        !document["counts"].contains("expanded") ||
        !ReadBoundedNonnegativeInteger(
            document["counts"]["expanded"],
            static_cast<uint64_t>(std::numeric_limits<int>::max()),
            &expanded_count) ||
        expanded_count != static_cast<uint64_t>(expected_count) ||
        !document["counts"].contains("vanilla") ||
        !ReadBoundedNonnegativeInteger(
            document["counts"]["vanilla"],
            static_cast<uint64_t>(std::numeric_limits<int>::max()),
            &vanilla_count) ||
        vanilla_count != 0) {
      return absl::FailedPreconditionError(
          absl::StrFormat("%s counts must declare vanilla=0 and expanded=%d",
                          label, expected_count));
    }
  }
  return bank;
}

std::string SerializeCanonicalBundle(const ExpandedSourceBank& bank) {
  Json document;
  document["format"] = "yaze-message-bundle";
  document["version"] = kMessageBundleVersion;
  document["counts"] = {{"vanilla", 0}, {"expanded", bank.size()}};
  document["messages"] = Json::array();
  for (const auto& [id, message] : bank) {
    document["messages"].push_back(
        {{"id", id}, {"bank", "expanded"}, {"text", message.text}});
  }
  return document.dump(2) + "\n";
}

std::string RenderAsmBody(const ExpandedSourceBank& bank,
                          int first_expanded_id) {
  std::string output;
  for (const auto& [local_id, message] : bank) {
    absl::StrAppend(&output, absl::StrFormat("Message_%03X:\n",
                                             first_expanded_id + local_id));
    std::vector<uint8_t> terminated = message.bytes;
    terminated.push_back(kMessageTerminator);
    for (size_t offset = 0; offset < terminated.size(); offset += 16) {
      absl::StrAppend(&output, "  db ");
      const size_t line_end = std::min(terminated.size(), offset + 16);
      for (size_t index = offset; index < line_end; ++index) {
        if (index != offset) {
          absl::StrAppend(&output, ", ");
        }
        absl::StrAppend(&output, absl::StrFormat("$%02X", terminated[index]));
      }
      absl::StrAppend(&output, "\n");
    }
    absl::StrAppend(&output, "\n");
  }
  absl::StrAppend(&output, "db $FF\n");
  return output;
}

std::string RenderAsmInclude(const ExpandedSourceBank& bank,
                             int first_expanded_id,
                             std::string_view source_sha256) {
  const std::string body = RenderAsmBody(bank, first_expanded_id);
  return absl::StrFormat(
             "; Source bundle SHA-256: %s\n"
             "; Generated ASM body SHA-256: %s\n"
             "; Generated by yaze message-source-sync. Do not edit.\n\n",
             std::string(source_sha256),
             core::ComputeSourceArtifactSha256(body)) +
         body;
}

bool IsCanonicalMappedLoRomAddress(uint32_t address) {
  const uint8_t bank = static_cast<uint8_t>((address >> 16) & 0xFFu);
  return bank != 0x7E && bank != 0x7F && (address & 0xFFFFu) >= 0x8000u &&
         PcToSnes(SnesToPc(address)) == address;
}

absl::StatusOr<size_t> ValidateLayoutAndGetCapacity(
    const core::MessageLayout& layout) {
  if (layout.expanded_count <= 0 ||
      layout.last_expanded_id < layout.first_expanded_id) {
    return absl::FailedPreconditionError(
        "Hack manifest must define a non-empty expanded message range");
  }
  const int range_count =
      static_cast<int>(layout.last_expanded_id - layout.first_expanded_id) + 1;
  if (layout.expanded_count != range_count) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Hack manifest expanded range is not contiguous: first=0x%03X, "
        "last=0x%03X implies %d messages, but count=%d",
        layout.first_expanded_id, layout.last_expanded_id, range_count,
        layout.expanded_count));
  }
  if (!IsCanonicalMappedLoRomAddress(layout.data_start) ||
      !IsCanonicalMappedLoRomAddress(layout.data_end)) {
    return absl::FailedPreconditionError(
        "Hack manifest expanded data range must use canonical mapped LoROM "
        "addresses");
  }
  const uint32_t start_pc = SnesToPc(layout.data_start);
  const uint32_t end_pc = SnesToPc(layout.data_end);
  if (end_pc < start_pc) {
    return absl::FailedPreconditionError(
        "Hack manifest expanded data range ends before it starts");
  }
  return static_cast<size_t>(end_pc - start_pc) + 1;
}

bool IsWithinRoot(const fs::path& root, const fs::path& candidate) {
  const fs::path relative = candidate.lexically_relative(root);
  if (relative.empty() || relative.is_absolute()) {
    return false;
  }
  const auto first = relative.begin();
  return first != relative.end() && *first != "..";
}

absl::StatusOr<fs::path> ResolveProjectTarget(
    const fs::path& project_root, const std::string& configured_path,
    bool must_exist, absl::string_view label) {
  const fs::path relative(configured_path);
  if (relative.empty() || relative.is_absolute() || relative.has_root_name()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s must be a non-empty project-relative path", label));
  }

  const fs::path joined = (project_root / relative).lexically_normal();
  std::error_code status_ec;
  const auto link_status = fs::symlink_status(joined, status_ec);
  if (status_ec != std::errc::no_such_file_or_directory && status_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot inspect %s path %s: %s", label, joined.string(),
                        status_ec.message()));
  }
  if (!status_ec && fs::is_symlink(link_status)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "%s may not be a symbolic link: %s", label, joined.string()));
  }

  std::error_code canonical_ec;
  fs::path resolved = must_exist ? fs::canonical(joined, canonical_ec)
                                 : fs::weakly_canonical(joined, canonical_ec);
  if (canonical_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot resolve %s path %s: %s", label, joined.string(),
                        canonical_ec.message()));
  }
  resolved = resolved.lexically_normal();
  if (!IsWithinRoot(project_root, resolved)) {
    return absl::PermissionDeniedError(
        absl::StrFormat("%s escapes project root %s: %s", label,
                        project_root.string(), resolved.string()));
  }

  std::error_code parent_ec;
  const auto parent_status = fs::status(resolved.parent_path(), parent_ec);
  if (parent_ec || !fs::is_directory(parent_status)) {
    return absl::FailedPreconditionError(
        absl::StrFormat("%s parent directory must already exist: %s", label,
                        resolved.parent_path().string()));
  }
  if (must_exist) {
    std::error_code file_ec;
    if (!fs::is_regular_file(resolved, file_ec) || file_ec) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "%s must be an existing regular file: %s", label, resolved.string()));
    }
  } else if (!status_ec && fs::exists(link_status) &&
             !fs::is_regular_file(link_status)) {
    return absl::FailedPreconditionError(
        absl::StrFormat("%s must be a regular file or a new path: %s", label,
                        resolved.string()));
  }
  return resolved;
}

absl::StatusOr<fs::path> CanonicalProjectRoot(
    const project::YazeProject& project) {
  if (project.filepath.empty()) {
    return absl::FailedPreconditionError(
        "Project has no descriptor path; open the project before source sync");
  }
  std::error_code ec;
  fs::path descriptor = fs::absolute(project.filepath, ec);
  if (ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot resolve project descriptor %s: %s",
                        project.filepath, ec.message()));
  }
  fs::path root = fs::canonical(descriptor.parent_path(), ec);
  if (ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot resolve project root for %s: %s",
                        project.filepath, ec.message()));
  }
  return root.lexically_normal();
}

std::string LowercasePath(const fs::path& path) {
  return absl::AsciiStrToLower(path.generic_string());
}

absl::Status ValidateDistinctTargets(const fs::path& bundle_path,
                                     const fs::path& include_path) {
  if (bundle_path == include_path ||
      LowercasePath(bundle_path) == LowercasePath(include_path)) {
    return absl::InvalidArgumentError(
        "Canonical bundle and generated ASM include paths must be distinct");
  }
  std::error_code exists_ec;
  const bool include_exists = fs::exists(include_path, exists_ec);
  if (exists_ec) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Cannot inspect generated ASM include path: %s", exists_ec.message()));
  }
  if (include_exists) {
    std::error_code equivalent_ec;
    if (fs::equivalent(bundle_path, include_path, equivalent_ec)) {
      return absl::InvalidArgumentError(
          "Canonical bundle and generated ASM include paths alias each other");
    }
    if (equivalent_ec) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Cannot compare source publication paths: %s",
                          equivalent_ec.message()));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> NormalizeExpectedSha256(std::string hash) {
  if (hash.size() != 64 ||
      !std::all_of(hash.begin(), hash.end(),
                   [](unsigned char c) { return std::isxdigit(c) != 0; })) {
    return absl::InvalidArgumentError(
        "--expected-source-sha256 must be exactly 64 hexadecimal characters");
  }
  return absl::AsciiStrToLower(std::move(hash));
}

}  // namespace

std::string ComputeMessageSourceSha256(std::string_view content) {
  return core::ComputeSourceArtifactSha256(content);
}

absl::StatusOr<MessageSourceSyncResult> SyncMessageSource(
    const project::YazeProject& project, const fs::path& incoming_bundle_path,
    const MessageSourceSyncOptions& options) {
  if (!project.hack_manifest.loaded()) {
    return absl::FailedPreconditionError(
        "Project must load a hack manifest before message source sync");
  }
#if defined(__EMSCRIPTEN__)
  if (options.write) {
    return absl::FailedPreconditionError(
        "message-source-sync --write is unavailable in browser builds because "
        "durable atomic filesystem publication cannot be guaranteed");
  }
#endif
  const core::MessageLayout& layout = project.hack_manifest.message_layout();
  if (!layout.source.has_value()) {
    return absl::FailedPreconditionError(
        "Hack manifest does not define messages.source");
  }

  size_t capacity = 0;
  ASSIGN_OR_RETURN(capacity, ValidateLayoutAndGetCapacity(layout));
  fs::path project_root;
  ASSIGN_OR_RETURN(project_root, CanonicalProjectRoot(project));

  fs::path canonical_bundle_path;
  ASSIGN_OR_RETURN(
      canonical_bundle_path,
      ResolveProjectTarget(project_root, layout.source->canonical_bundle_path,
                           /*must_exist=*/true, "Canonical message bundle"));
  fs::path asm_include_path;
  ASSIGN_OR_RETURN(asm_include_path,
                   ResolveProjectTarget(
                       project_root, layout.source->generated_asm_include_path,
                       /*must_exist=*/false, "Generated message ASM include"));
  RETURN_IF_ERROR(
      ValidateDistinctTargets(canonical_bundle_path, asm_include_path));
  const std::vector<fs::path> publication_targets = {canonical_bundle_path,
                                                     asm_include_path};
  RETURN_IF_ERROR(core::ValidateSourceArtifactPublicationTargets(
      publication_targets, kMessageSourcePublisherLabels));

  std::error_code incoming_ec;
  const fs::path resolved_incoming =
      fs::canonical(incoming_bundle_path, incoming_ec);
  if (incoming_ec || !fs::is_regular_file(resolved_incoming, incoming_ec) ||
      incoming_ec) {
    return absl::NotFoundError(absl::StrFormat(
        "Incoming message bundle must be an existing regular file: %s",
        incoming_bundle_path.string()));
  }

  std::string expected_sha;
  std::unique_ptr<core::SourceArtifactPublicationLock> write_locks;
  if (options.write) {
    if (options.expected_source_sha256.empty()) {
      return absl::InvalidArgumentError(
          "--write requires --expected-source-sha256");
    }
    ASSIGN_OR_RETURN(expected_sha,
                     NormalizeExpectedSha256(options.expected_source_sha256));
    ASSIGN_OR_RETURN(write_locks,
                     core::AcquireSourceArtifactPublicationLock(
                         publication_targets, kMessageSourcePublisherLabels));
  }

  std::string canonical_before;
  ASSIGN_OR_RETURN(canonical_before, ReadTextFile(canonical_bundle_path,
                                                  "canonical message bundle"));
  const std::string source_sha_before =
      core::ComputeSourceArtifactSha256(canonical_before);
  Json canonical_document;
  ASSIGN_OR_RETURN(
      canonical_document,
      ParseBundleDocument(canonical_before, "Canonical message bundle"));
  ExpandedSourceBank merged_bank;
  ASSIGN_OR_RETURN(
      merged_bank,
      ParseExpandedBank(canonical_document, layout.expanded_count,
                        /*require_complete=*/true, "Canonical message bundle"));
  const std::string expected_current_asm = RenderAsmInclude(
      merged_bank, layout.first_expanded_id, source_sha_before);

  std::string incoming_content;
  ASSIGN_OR_RETURN(incoming_content,
                   ReadTextFile(resolved_incoming, "incoming message bundle"));
  Json incoming_document;
  ASSIGN_OR_RETURN(
      incoming_document,
      ParseBundleDocument(incoming_content, "Incoming message bundle"));
  ExpandedSourceBank incoming_bank;
  ASSIGN_OR_RETURN(
      incoming_bank,
      ParseExpandedBank(incoming_document, layout.expanded_count,
                        /*require_complete=*/false, "Incoming message bundle"));
  for (auto& [id, message] : incoming_bank) {
    merged_bank[id] = std::move(message);
  }

  size_t encoded_size = 1;  // Final $FF.
  for (const auto& entry : merged_bank) {
    encoded_size += entry.second.bytes.size() + 1;  // Per-message $7F.
  }
  if (encoded_size > capacity) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Expanded message source needs %zu bytes including terminators, but "
        "manifest capacity is %zu bytes [0x%06X, 0x%06X]",
        encoded_size, capacity, layout.data_start, layout.data_end));
  }

  const std::string canonical_after = SerializeCanonicalBundle(merged_bank);
  const std::string proposed_source_sha =
      core::ComputeSourceArtifactSha256(canonical_after);
  const std::string asm_after = RenderAsmInclude(
      merged_bank, layout.first_expanded_id, proposed_source_sha);

  std::optional<std::string> asm_before;
  std::error_code include_exists_ec;
  if (fs::exists(asm_include_path, include_exists_ec)) {
    std::string existing_include;
    ASSIGN_OR_RETURN(
        existing_include,
        ReadTextFile(asm_include_path, "generated message ASM include"));
    asm_before = std::move(existing_include);
  } else if (include_exists_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot inspect generated message ASM include: %s",
                        include_exists_ec.message()));
  }
  if (asm_before.has_value() && *asm_before != expected_current_asm) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Generated message ASM include drifted from the current canonical "
        "bundle: %s",
        asm_include_path.string()));
  }

  MessageSourceSyncResult result{
      .canonical_bundle_path = canonical_bundle_path,
      .generated_asm_include_path = asm_include_path,
      .first_expanded_id = layout.first_expanded_id,
      .last_expanded_id = layout.last_expanded_id,
      .expanded_count = layout.expanded_count,
      .incoming_updates = static_cast<int>(incoming_bank.size()),
      .encoded_size = encoded_size,
      .capacity = capacity,
      .source_sha256_before = source_sha_before,
      .proposed_source_sha256 = proposed_source_sha,
      .changed = canonical_before != canonical_after ||
                 !asm_before.has_value() || *asm_before != asm_after,
  };
  if (!options.write) {
    return result;
  }
  if (expected_sha != source_sha_before) {
    return absl::AbortedError(absl::StrFormat(
        "Canonical message source SHA-256 CAS failed: expected %s, got %s",
        expected_sha, source_sha_before));
  }
  if (!result.changed) {
    return result;
  }

  std::vector<core::SourceArtifactUpdate> updates;
  updates.push_back(core::SourceArtifactUpdate{
      .target = canonical_bundle_path,
      .before = canonical_before,
      .after = canonical_after,
  });
  updates.push_back(core::SourceArtifactUpdate{
      .target = asm_include_path,
      .before = asm_before,
      .after = asm_after,
  });
  RETURN_IF_ERROR(core::PublishSourceArtifacts(
      *write_locks, std::move(updates), expected_sha, [&]() -> absl::Status {
        // Reopen the canonical bundle through the strict source parser, not
        // just as bytes, before reporting a successful two-file publication.
        std::string reopened_canonical;
        ASSIGN_OR_RETURN(reopened_canonical,
                         ReadTextFile(canonical_bundle_path,
                                      "published canonical message bundle"));
        Json reopened_document;
        ASSIGN_OR_RETURN(
            reopened_document,
            ParseBundleDocument(reopened_canonical,
                                "Published canonical message bundle"));
        ExpandedSourceBank reopened_bank;
        ASSIGN_OR_RETURN(
            reopened_bank,
            ParseExpandedBank(reopened_document, layout.expanded_count,
                              /*require_complete=*/true,
                              "Published canonical message bundle"));
        if (core::ComputeSourceArtifactSha256(reopened_canonical) !=
            proposed_source_sha) {
          return absl::DataLossError(
              "Published canonical message bundle SHA-256 readback failed");
        }
        return absl::OkStatus();
      }));
  result.wrote = true;
  return result;
}

}  // namespace yaze::editor
