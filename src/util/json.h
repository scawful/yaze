#ifndef YAZE_UTIL_JSON_H
#define YAZE_UTIL_JSON_H

#include "yaze_config.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
namespace yaze {
using Json = nlohmann::json;
}
#else
#include <stdexcept>
#include <string>
#include <vector>
#include <map>

namespace yaze {

// Stub implementation of nlohmann::json interface to allow compilation without JSON support
class Json {
 public:
  // Construction
  Json() = default;
  Json(std::nullptr_t) {}
  Json(bool) {}
  Json(int) {}
  Json(double) {}
  Json(const char*) {}
  Json(const std::string&) {}
  // Templated constructor to catch other types
  template <typename T> Json(const T&) {}

  // Static factories
  static Json object() { return Json(); }
  static Json array() { return Json(); }
  static Json parse(const std::string&) { throw std::runtime_error("JSON support disabled"); }

  // Assignment
  template <typename T> Json& operator=(const T&) { return *this; }

  // Access
  Json& operator[](const std::string&) { return *this; }
  const Json& operator[](const std::string&) const { return *this; }
  Json& operator[](size_t) { return *this; }
  const Json& operator[](size_t) const { return *this; }
  Json& operator[](int) { return *this; }
  const Json& operator[](int) const { return *this; }
  
  template <typename T> T get() const { return T(); }
  template <typename T> T value(const std::string&, const T& def) const { return def; }

  // Query
  bool contains(const std::string&) const { return false; }
  bool is_null() const { return true; }
  bool is_boolean() const { return false; }
  bool is_number() const { return false; }
  bool is_object() const { return false; }
  bool is_array() const { return false; }
  bool is_string() const { return false; }
  bool is_discarded() const { return true; }
  size_t size() const { return 0; }
  bool empty() const { return true; }

  // Iteration
  struct iterator {
    bool operator!=(const iterator&) const { return false; }
    void operator++() {}
    Json& operator*() { static Json j; return j; }
    std::string key() const { return ""; }
    Json& value() { static Json j; return j; }
  };
  struct const_iterator {
    bool operator!=(const const_iterator&) const { return false; }
    void operator++() {}
    const Json& operator*() const { static Json j; return j; }
  };

  iterator begin() { return {}; }
  iterator end() { return {}; }
  const_iterator begin() const { return {}; }
  const_iterator end() const { return {}; }

  // Items
  struct items_view {
    iterator begin() { return {}; }
    iterator end() { return {}; }
  };
  items_view items() { return {}; }

  // Output
  std::string dump(int = -1, char = ' ', bool = false, int = 0) const { return "{}"; }
  
  // Exceptions
  class exception : public std::exception {
   public:
    const char* what() const noexcept override { return "JSON error"; }
  };
};

}  // namespace yaze
#endif

#endif  // YAZE_UTIL_JSON_H

