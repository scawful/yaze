#ifndef YAZE_UTIL_FLAG_H_
#define YAZE_UTIL_FLAG_H_

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace util {

namespace detail {
[[noreturn]] void FlagParseFatal(const std::string& message);
}

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
      detail::FlagParseFatal("Failed to parse flag: " + name_);
    }
    value_ = parsed;
  }
  
  // Set the value directly (used by specializations)
  void SetValue(const T& val) { value_ = val; }

  // Returns the current (parsed or default) value of the flag.
  const T& Get() const { return value_; }

 private:
  std::string name_;
  T value_;
  T default_;
  std::string help_;
};

// Specialization for bool to handle "true"/"false" strings
template <>
inline void Flag<bool>::ParseValue(const std::string& text) {
  if (text == "true" || text == "1" || text == "yes" || text == "on") {
    SetValue(true);
  } else if (text == "false" || text == "0" || text == "no" || text == "off") {
    SetValue(false);
  } else {
    detail::FlagParseFatal("Failed to parse boolean flag: " + name() +
                           " (expected true/false/1/0/yes/no/on/off, got: " +
                           text + ")");
  }
}

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
  void Parse(std::vector<std::string>* tokens);

 private:
  FlagRegistry* registry_;

  // Checks if there is an '=' sign in the token, extracting flag name and
  // value. e.g. "--count=42" -> flag_name = "--count", value_string = "42"
  // returns true if '=' was found
  bool ExtractFlagAndValue(const std::string& token, std::string* flag_name,
                           std::string* value_string);

  // Mode flag '-'
  bool ExtractFlag(const std::string& token, std::string* flag_name);
};

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_FLAG_H_
