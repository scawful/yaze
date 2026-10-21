#include "app/application.h"

namespace yaze {

Application& Application::Instance() {
  static Application instance;
  return instance;
}

}  // namespace yaze
