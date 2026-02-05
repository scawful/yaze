#ifndef YAZE_SRC_CLI_HANDLERS_MESSAGE_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_MESSAGE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for listing messages
 */
class MessageListCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "message-list"; }
  std::string GetDescription() const { return "List available messages"; }
  std::string GetUsage() const {
    return "message-list [--limit <limit>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for reading messages
 */
class MessageReadCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "message-read"; }
  std::string GetDescription() const { return "Read a specific message"; }
  std::string GetUsage() const {
    return "message-read --id <message_id> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"id"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for searching messages
 */
class MessageSearchCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "message-search"; }
  std::string GetDescription() const {
    return "Search messages by text content";
  }
  std::string GetUsage() const {
    return "message-search --query <query> [--limit <limit>] [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"query"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Encode a human-readable message string to ROM bytes
 *
 * Takes text with [command] tokens and produces raw byte sequence.
 * Does not require a ROM — encoding uses static tables only.
 */
class MessageEncodeCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "message-encode"; }
  std::string GetUsage() const override {
    return "message-encode --text <text> [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"text"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Decode raw ROM bytes (hex string) to human-readable text
 *
 * Takes a hex byte string and produces text with [command] tokens.
 * Does not require a ROM — decoding uses static tables only.
 */
class MessageDecodeCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "message-decode"; }
  std::string GetUsage() const override {
    return "message-decode --hex <hex_bytes> [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"hex"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Import messages from .org format file
 *
 * Parses a messages.org file and encodes each message to bytes.
 * Reports results including any validation warnings.
 */
class MessageImportOrgCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "message-import-org"; }
  std::string GetUsage() const override {
    return "message-import-org --file <path> [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"file"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Export messages from ROM as .org format
 *
 * Reads all messages from the ROM and exports them as .org format text.
 */
class MessageExportOrgCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "message-export-org"; }
  std::string GetUsage() const override {
    return "message-export-org --output <path> [--format <json|text>]";
  }
  bool RequiresRom() const override { return true; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"output"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Export messages as a bundle JSON for round-trip editing
 */
class MessageExportBundleCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "message-export-bundle"; }
  std::string GetUsage() const override {
    return "message-export-bundle --output <path> [--range <all|vanilla|expanded>]"
           " [--format <json|text>]";
  }
  bool RequiresRom() const override { return true; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"output"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Import messages from a bundle JSON for validation or apply to ROM
 */
class MessageImportBundleCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "message-import-bundle"; }
  std::string GetUsage() const override {
    return "message-import-bundle --file <path> [--apply] [--strict]"
           " [--range <all|vanilla|expanded>] [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"file"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Write an encoded message to the expanded message bank
 *
 * Encodes text and writes to the expanded message region at a given ID.
 */
class MessageWriteCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "message-write"; }
  std::string GetUsage() const override {
    return "message-write --id <id> --text <text> [--format <json|text>]";
  }
  bool RequiresRom() const override { return true; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"id", "text"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Export expanded message region as binary or assembly
 */
class MessageExportBinCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "message-export-bin"; }
  std::string GetUsage() const override {
    return "message-export-bin --output <path> [--range expanded] "
           "[--format <json|text>]";
  }
  bool RequiresRom() const override { return true; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"output"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Export expanded message region as assembly source
 */
class MessageExportAsmCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "message-export-asm"; }
  std::string GetUsage() const override {
    return "message-export-asm --output <path> [--range expanded] "
           "[--format <json|text>]";
  }
  bool RequiresRom() const override { return true; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"output"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_MESSAGE_COMMANDS_H_
