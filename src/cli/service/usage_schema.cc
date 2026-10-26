#include "cli/service/usage_schema.h"

#include <algorithm>
#include <cstddef>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

namespace yaze::cli {

namespace {

enum class TokenKind { kOpenOptional, kOpenGroup, kClose, kBar, kWord };

struct Token {
  TokenKind kind;
  std::string text;
};

bool IsGroupDelimiter(char c) {
  return c == '[' || c == ']' || c == '(' || c == ')';
}

bool IsChoiceChar(char c) {
  return absl::ascii_isalnum(static_cast<unsigned char>(c)) || c == '_' ||
         c == '-' || c == '.' || c == '+';
}

bool IsFlagWord(absl::string_view word) {
  return word.size() > 1 && word[0] == '-' && word != "--";
}

// Reads one word, keeping `<...>` placeholders intact even when they contain
// spaces or `|` ("<code folder|.yaze>").
std::string ReadWord(absl::string_view usage, size_t& pos) {
  std::string word;
  while (pos < usage.size()) {
    const char c = usage[pos];
    if (c == '<') {
      size_t end = usage.find('>', pos);
      if (end == absl::string_view::npos) {
        end = usage.size() - 1;
      }
      word.append(usage.substr(pos, end - pos + 1));
      pos = end + 1;
      continue;
    }
    if (absl::ascii_isspace(static_cast<unsigned char>(c)) ||
        IsGroupDelimiter(c)) {
      break;
    }
    // "--sandbox|--mock-rom" alternates flags; "json|text" is one value.
    if (c == '|' && pos + 1 < usage.size() && usage[pos + 1] == '-') {
      break;
    }
    word.push_back(c);
    ++pos;
  }
  return word;
}

std::vector<Token> Tokenize(absl::string_view usage) {
  std::vector<Token> tokens;
  size_t pos = 0;
  while (pos < usage.size()) {
    const char c = usage[pos];
    if (absl::ascii_isspace(static_cast<unsigned char>(c))) {
      ++pos;
      continue;
    }
    if (c == '[' || c == '(') {
      tokens.push_back(
          {c == '[' ? TokenKind::kOpenOptional : TokenKind::kOpenGroup, ""});
      ++pos;
      continue;
    }
    if (c == ']' || c == ')') {
      tokens.push_back({TokenKind::kClose, ""});
      ++pos;
      continue;
    }
    if (c == '|') {
      tokens.push_back({TokenKind::kBar, ""});
      ++pos;
      continue;
    }

    tokens.push_back({TokenKind::kWord, ReadWord(usage, pos)});
  }
  return tokens;
}

std::string NormalizeHint(absl::string_view hint) {
  absl::ConsumeSuffix(&hint, "...");
  if (hint.size() >= 2 && hint.front() == '<' && hint.back() == '>') {
    hint = hint.substr(1, hint.size() - 2);
  }
  absl::ConsumeSuffix(&hint, "...");
  return std::string(absl::StripAsciiWhitespace(hint));
}

std::vector<std::string> ExtractChoices(absl::string_view hint) {
  if (!absl::StrContains(hint, '|')) {
    return {};
  }
  std::vector<std::string> choices = absl::StrSplit(hint, '|');
  for (const std::string& choice : choices) {
    if (choice.empty() ||
        !std::all_of(choice.begin(), choice.end(), IsChoiceChar)) {
      return {};
    }
  }
  return choices;
}

bool IsValidName(absl::string_view name) {
  return !name.empty() &&
         absl::ascii_isalnum(static_cast<unsigned char>(name.front())) &&
         std::all_of(name.begin(), name.end(), [](char c) {
           return absl::ascii_isalnum(static_cast<unsigned char>(c)) ||
                  c == '_' || c == '-';
         });
}

std::string PositionalName(absl::string_view placeholder) {
  std::string name;
  for (char c : NormalizeHint(placeholder)) {
    name.push_back(absl::ascii_isalnum(static_cast<unsigned char>(c)) ? c
                                                                      : '_');
  }
  return name;
}

bool IsValueWord(const Token& token) {
  return token.kind == TokenKind::kWord && !token.text.empty() &&
         token.text[0] != '-' && token.text != "...";
}

}  // namespace

std::vector<UsageParameter> ParseUsageParameters(absl::string_view usage) {
  std::vector<Token> tokens = Tokenize(usage);

  size_t index = 0;
  if (index < tokens.size() && tokens[index].kind == TokenKind::kWord &&
      tokens[index].text == "z3ed") {
    ++index;
  }
  if (index < tokens.size() && IsValueWord(tokens[index]) &&
      tokens[index].text[0] != '<') {
    ++index;
  }

  std::vector<UsageParameter> parameters;
  std::vector<bool> group_is_optional;
  bool after_bar = false;

  auto add = [&](UsageParameter parameter) {
    parameter.required =
        !after_bar &&
        std::none_of(group_is_optional.begin(), group_is_optional.end(),
                     [](bool optional) { return optional; });
    after_bar = false;
    const bool duplicate = std::any_of(
        parameters.begin(), parameters.end(), [&](const UsageParameter& p) {
          return p.name == parameter.name &&
                 p.positional == parameter.positional;
        });
    if (!duplicate) {
      parameters.push_back(std::move(parameter));
    }
  };

  for (; index < tokens.size(); ++index) {
    const Token& token = tokens[index];
    switch (token.kind) {
      case TokenKind::kOpenOptional:
      case TokenKind::kOpenGroup:
        group_is_optional.push_back(token.kind == TokenKind::kOpenOptional);
        break;
      case TokenKind::kClose:
        if (!group_is_optional.empty()) {
          group_is_optional.pop_back();
        }
        break;
      case TokenKind::kBar:
        if (!parameters.empty()) {
          parameters.back().required = false;
        }
        after_bar = true;
        break;
      case TokenKind::kWord: {
        absl::string_view word = token.text;
        if (IsFlagWord(word)) {
          if (!absl::ConsumePrefix(&word, "--")) {
            absl::ConsumePrefix(&word, "-");
          }
          UsageParameter parameter;
          const size_t equals = word.find('=');
          if (equals != absl::string_view::npos) {
            parameter.takes_value = true;
            parameter.value_hint = NormalizeHint(word.substr(equals + 1));
            word = word.substr(0, equals);
          } else if (index + 1 < tokens.size() &&
                     IsValueWord(tokens[index + 1])) {
            parameter.takes_value = true;
            parameter.value_hint = NormalizeHint(tokens[++index].text);
          }
          if (!IsValidName(word)) {
            break;
          }
          parameter.name = std::string(word);
          parameter.choices = ExtractChoices(parameter.value_hint);
          add(std::move(parameter));
        } else if (word.front() == '<') {
          UsageParameter parameter;
          parameter.name = PositionalName(word);
          parameter.positional = true;
          parameter.takes_value = true;
          if (IsValidName(parameter.name)) {
            add(std::move(parameter));
          }
        }
        break;
      }
    }
  }
  return parameters;
}

nlohmann::ordered_json UsageParametersToJsonSchema(
    const std::vector<UsageParameter>& parameters) {
  nlohmann::ordered_json properties = nlohmann::ordered_json::object();
  nlohmann::ordered_json required = nlohmann::ordered_json::array();
  for (const UsageParameter& parameter : parameters) {
    if (parameter.positional) {
      continue;
    }
    nlohmann::ordered_json property;
    property["type"] = parameter.takes_value ? "string" : "boolean";
    if (!parameter.choices.empty()) {
      property["enum"] = parameter.choices;
    } else if (!parameter.value_hint.empty()) {
      property["description"] = parameter.value_hint;
    }
    properties[parameter.name] = std::move(property);
    if (parameter.required) {
      required.push_back(parameter.name);
    }
  }

  nlohmann::ordered_json schema;
  schema["type"] = "object";
  schema["properties"] = std::move(properties);
  if (!required.empty()) {
    schema["required"] = std::move(required);
  }
  return schema;
}

}  // namespace yaze::cli
