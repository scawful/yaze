#ifndef YAZE_UTIL_FLAG_H_
#define YAZE_UTIL_FLAG_H_

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace util {

// Base interface for all flags.
class IFlag {
 public:
  virtual ~IFlag() = default;

  // Returns the full name (e.g. "--count") used for this flag.
  virtual const std::string& name() const = 0;

  // Returns help text describing how to use this flag.
  virtual const std::string& help() const = 0;

  // Parses a string value into the underlying type.
  virtual void ParseValue(const std::string& text) = 0;
};

template <typename T>
class Flag : public IFlag {
 public:
  Flag(const std::string& name, const T& default_value,
       const std::string& help_text)
      : name_(name),
        value_(default_value),
        default_(default_value),
        help_(help_text) {}

  const std::string& name() const override { return name_; }
  const std::string& help() const override { return help_; }

  // Attempts to parse a string into type T using a stringstream.
  void ParseValue(const std::string& text) override {
    std::stringstream ss(text);
    T parsed;
    if (!(ss >> parsed)) {
      throw std::runtime_error("Failed to parse flag: " + name_);
    }
    value_ = parsed;
  }

  // Returns the current (parsed or default) value of the flag.
  const T& Get() const { return value_; }

 private:
  std::string name_;
  T value_;
  T default_;
  std::string help_;
};

class FlagRegistry {
 public:
  // Registers a flag in the global registry.
  // The return type is a pointer to the newly created flag.
  template <typename T>
  Flag<T>* RegisterFlag(const std::string& name, const T& default_value,
                        const std::string& help_text) {
    auto flag = std::make_unique<Flag<T>>(name, default_value, help_text);
    Flag<T>* raw_ptr =
        flag.get();  // We keep a non-owning pointer to use later.
    flags_[name] = std::move(flag);
    return raw_ptr;
  }

  // Returns a shared interface pointer if found, otherwise nullptr.
  IFlag* GetFlag(const std::string& name) const {
    auto it = flags_.find(name);
    if (it == flags_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

  // Returns all registered flags for iteration, help text, etc.
  std::vector<IFlag*> AllFlags() const {
    std::vector<IFlag*> result;
    result.reserve(flags_.size());
    for (auto const& kv : flags_) {
      result.push_back(kv.second.get());
    }
    return result;
  }

 private:
  std::unordered_map<std::string, std::unique_ptr<IFlag>> flags_;
};

inline FlagRegistry* global_flag_registry() {
  // Guaranteed to be initialized once per process.
  static FlagRegistry* registry = new FlagRegistry();
  return registry;
}

#define DECLARE_FLAG(type, name) extern yaze::util::Flag<type>* FLAGS_##name

// Defines a global Flag<type>* FLAGS_<name> and registers it.
#define DEFINE_FLAG(type, name, default_val, help_text)       \
  yaze::util::Flag<type>* FLAGS_##name =                      \
      yaze::util::global_flag_registry()->RegisterFlag<type>( \
          "--" #name, (default_val), (help_text))

// Retrieves the current value of a declared flag.
#define FLAG_VALUE(name) (FLAGS_##name->Get())

class FlagParser {
 public:
  explicit FlagParser(FlagRegistry* registry) : registry_(registry) {}

  // Parses flags out of the given command line arguments.
  void Parse(int argc, char** argv) {
    std::vector<std::string> tokens;
    for (int i = 0; i < argc; i++) {
      tokens.push_back(argv[i]);
    }
    Parse(&tokens);
  }

  // Parses flags out of the given token list. Recognizes forms:
  // --flag=value or --flag value
  // Any token not recognized as a flag is left in `leftover`.
  void Parse(std::vector<std::string>* tokens) {
    std::vector<std::string> leftover;
    leftover.reserve(tokens->size());

    for (size_t i = 0; i < tokens->size(); i++) {
      const std::string& token = (*tokens)[i];
      if (token.rfind("--", 0) == 0) {
        // Found a token that starts with "--".
        std::string flag_name;
        std::string value_string;
        if (!ExtractFlagAndValue(token, &flag_name, &value_string)) {
          // If no value found after '=', see if next token is a value.
          if ((i + 1) < tokens->size()) {
            const std::string& next_token = (*tokens)[i + 1];
            // If next token is NOT another flag, treat it as the value.
            if (next_token.rfind("--", 0) != 0) {
              value_string = next_token;
              i++;
            } else {
              // If no explicit value, treat it as boolean 'true'.
              value_string = "true";
            }
          } else {
            value_string = "true";
          }
          flag_name = token;
        }

        // Attempt to parse the flag (strip leading dashes in the registry).
        IFlag* flag_ptr = registry_->GetFlag(flag_name);
        if (!flag_ptr) {
          throw std::runtime_error("Unrecognized flag: " + flag_name);
        }

        // Set the parsed value on the matching flag.
        flag_ptr->ParseValue(value_string);
      } else {
        leftover.push_back(token);
      }
    }
    *tokens = leftover;
  }

 private:
  FlagRegistry* registry_;

  // Checks if there is an '=' sign in the token, extracting flag name and
  // value. e.g. "--count=42" -> flag_name = "--count", value_string = "42"
  // returns true if '=' was found
  bool ExtractFlagAndValue(const std::string& token, std::string* flag_name,
                           std::string* value_string) {
    const size_t eq_pos = token.find('=');
    if (eq_pos == std::string::npos) {
      return false;
    }
    *flag_name = token.substr(0, eq_pos);
    *value_string = token.substr(eq_pos + 1);
    return true;
  }
};

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_FLAG_H_
