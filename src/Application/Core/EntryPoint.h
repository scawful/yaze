#ifndef YAZE_APPLICATION_CONTROLLER_ENTRYPOINT_H
#define YAZE_APPLICATION_CONTROLLER_ENTRYPOINT_H

#include "Controller.h"

int main(int argc, char** argv) {
  yaze::Application::Core::Controller controller;

  controller.onEntry();
  while (controller.isActive()) {
    controller.onInput();
    controller.onLoad();
    controller.doRender();
  }
  controller.onExit();

  return EXIT_SUCCESS;
}

#endif  // YAZE_APPLICATION_CONTROLLER_ENTRYPOINT_H