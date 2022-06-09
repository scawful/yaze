#ifndef YAZE_APPLICATION_CONTROLLER_ENTRYPOINT_H
#define YAZE_APPLICATION_CONTROLLER_ENTRYPOINT_H

#include "Controller.h"

using namespace yaze::Application::Core;

int main(int argc, char** argv) 
{
  Controller controller;

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