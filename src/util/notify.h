#ifndef YAZE_UTIL_NOTIFY_H
#define YAZE_UTIL_NOTIFY_H

namespace yaze {
namespace util {

/**
 * @class NotifyValue
 * @brief A class to manage a value that can be modified and notify when it
 * changes.
 */
template <typename T>
class NotifyValue {
 public:
  NotifyValue() : value_(), modified_(false), temp_value_() {}
  NotifyValue(const T& value)
      : value_(value), modified_(false), temp_value_() {}

  void set(const T& value) {
    if (value != value_) {
      value_ = value;
      modified_ = true;
    }
  }

  void set(T&& value) {
    if (value != value_) {
      value_ = std::move(value);
      modified_ = true;
    }
  }

  const T& get() {
    modified_ = false;
    return value_;
  }

  T& edit() {
    modified_ = false;
    temp_value_ = value_;
    return temp_value_;
  }

  void commit() {
    if (temp_value_ != value_) {
      value_ = temp_value_;
      modified_ = true;
    }
  }

  bool consume_modified() {
    bool modified = modified_;
    modified_ = false;
    return modified;
  }

  operator T() { return get(); }
  void operator=(const T& value) { set(value); }
  bool modified() const { return modified_; }

 private:
  T value_;
  T temp_value_;
  bool modified_;
};

}  // namespace util
}  // namespace yaze

#endif