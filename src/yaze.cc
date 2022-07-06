#include "yaze.h"

int main(int argc, char** argv) {
  yaze::app::core::Controller controller;
  controller.onEntry();
  while (controller.isActive()) {
    controller.onInput();
    controller.onLoad();
    controller.doRender();
  }
  controller.onExit();
  return EXIT_SUCCESS;
}