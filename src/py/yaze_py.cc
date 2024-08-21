#include <boost/python.hpp>

#include "base/overworld.h"
#include "base/snes_color.h"
#include "base/sprite.h"
#include "base/extension.h"
#include "yaze.h"

class PythonYazeExtensionWrapper {
 public:
  PythonYazeExtensionWrapper() : ext_() {
    // Initialize function pointers to nullptr or default implementations
    ext_.initialize = nullptr;
    ext_.cleanup = nullptr;
    ext_.manipulate_rom = nullptr;
    ext_.extend_ui = nullptr;
    ext_.register_commands = nullptr;
    ext_.register_custom_tools = nullptr;
    ext_.register_event_hooks = nullptr;
  }

  const char* getName() const { return ext_.name; }

  const char* getVersion() const { return ext_.version; }

  void setName(const std::string& name) { ext_.name = name.c_str(); }

  void setVersion(const std::string& version) {
    ext_.version = version.c_str();
  }

  void setInitialize(boost::python::object func) {
    ext_.initialize = &PythonYazeExtensionWrapper::initialize;
    initializeFunc_ = func;
  }

  void setCleanup(boost::python::object func) {
    ext_.cleanup = &PythonYazeExtensionWrapper::cleanup;
    cleanupFunc_ = func;
  }

  void setManipulateRom(boost::python::object func) {
    ext_.manipulate_rom = &PythonYazeExtensionWrapper::manipulate_rom;
    manipulateRomFunc_ = func;
  }

  void setExtendUI(boost::python::object func) {
    ext_.extend_ui = &PythonYazeExtensionWrapper::extend_ui;
    extendUIFunc_ = func;
  }

  void setRegisterCommands(boost::python::object func) {
    ext_.register_commands = &PythonYazeExtensionWrapper::register_commands;
    registerCommandsFunc_ = func;
  }

  void setRegisterCustomTools(boost::python::object func) {
    ext_.register_custom_tools =
        &PythonYazeExtensionWrapper::register_custom_tools;
    registerCustomToolsFunc_ = func;
  }

  void setRegisterEventHooks(boost::python::object func) {
    ext_.register_event_hooks =
        &PythonYazeExtensionWrapper::register_event_hooks;
    registerEventHooksFunc_ = func;
  }

  yaze_extension* getExtension() { return &ext_; }

 private:
  static void initialize(yaze_editor_context* context) {
    boost::python::call<void>(initializeFunc_.ptr(), boost::python::ptr(context));
  }

  static void cleanup() { boost::python::call<void>(cleanupFunc_.ptr()); }

  static void manipulate_rom(z3_rom* rom) {
    boost::python::call<void>(manipulateRomFunc_.ptr(), boost::python::ptr(rom));
  }

  static void extend_ui(yaze_editor_context* context) {
    boost::python::call<void>(extendUIFunc_.ptr(), boost::python::ptr(context));
  }

  static void register_commands() {
    boost::python::call<void>(registerCommandsFunc_.ptr());
  }

  static void register_custom_tools() {
    boost::python::call<void>(registerCustomToolsFunc_.ptr());
  }

  static void register_event_hooks(yaze_event_type event,
                                   yaze_event_hook_func hook) {
    boost::python::call<void>(registerEventHooksFunc_.ptr(), event, hook);
  }

  yaze_extension ext_;
  static boost::python::object initializeFunc_;
  static boost::python::object cleanupFunc_;
  static boost::python::object manipulateRomFunc_;
  static boost::python::object extendUIFunc_;
  static boost::python::object registerCommandsFunc_;
  static boost::python::object registerCustomToolsFunc_;
  static boost::python::object registerEventHooksFunc_;
};

// Static members initialization
boost::python::object PythonYazeExtensionWrapper::initializeFunc_;
boost::python::object PythonYazeExtensionWrapper::cleanupFunc_;
boost::python::object PythonYazeExtensionWrapper::manipulateRomFunc_;
boost::python::object PythonYazeExtensionWrapper::extendUIFunc_;
boost::python::object PythonYazeExtensionWrapper::registerCommandsFunc_;
boost::python::object PythonYazeExtensionWrapper::registerCustomToolsFunc_;
boost::python::object PythonYazeExtensionWrapper::registerEventHooksFunc_;

BOOST_PYTHON_MODULE(yaze_py) {
  using namespace boost::python;

  class_<PythonYazeExtensionWrapper>("YazeExtension")
      .add_property("name", &PythonYazeExtensionWrapper::getName,
                    &PythonYazeExtensionWrapper::setName)
      .add_property("version", &PythonYazeExtensionWrapper::getVersion,
                    &PythonYazeExtensionWrapper::setVersion)
      .def("setInitialize", &PythonYazeExtensionWrapper::setInitialize)
      .def("setCleanup", &PythonYazeExtensionWrapper::setCleanup)
      .def("setManipulateRom", &PythonYazeExtensionWrapper::setManipulateRom)
      .def("setExtendUI", &PythonYazeExtensionWrapper::setExtendUI)
      .def("setRegisterCommands",
           &PythonYazeExtensionWrapper::setRegisterCommands)
      .def("setRegisterCustomTools",
           &PythonYazeExtensionWrapper::setRegisterCustomTools)
      .def("setRegisterEventHooks",
           &PythonYazeExtensionWrapper::setRegisterEventHooks)
      .def("getExtension", &PythonYazeExtensionWrapper::getExtension,
           return_value_policy<reference_existing_object>());

  class_<z3_rom>("z3_rom")
      .def_readonly("filename", &z3_rom::filename)
      .def_readonly("data", &z3_rom::data)
      .def_readonly("size", &z3_rom::size)
      .def_readonly("impl", &z3_rom::impl);

  class_<snes_color>("snes_color")
      .def_readonly("red", &snes_color::red)
      .def_readonly("green", &snes_color::green)
      .def_readonly("blue", &snes_color::blue);

  class_<snes_palette>("snes_palette")
      .def_readonly("id", &snes_palette::id)
      .def_readonly("size", &snes_palette::size)
      .def_readonly("colors", &snes_palette::colors);

  class_<z3_sprite_action>("z3_sprite_action")
      .def_readonly("name", &z3_sprite_action::name)
      .def_readonly("id", &z3_sprite_action::id);

  class_<z3_sprite>("sprite")
      .def_readonly("name", &z3_sprite::name)
      .def_readonly("id", &z3_sprite::id)
      .def_readonly("actions", &z3_sprite::actions);

  class_<yaze_flags>("yaze_flags")
      .def_readwrite("debug", &yaze_flags::debug)
      .def_readwrite("rom_filename", &yaze_flags::rom_filename)
      .def_readwrite("rom", &yaze_flags::rom);

  class_<yaze_project>("yaze_project")
      .def_readonly("filename", &yaze_project::filename)
      .def_readonly("rom", &yaze_project::rom)
      .def_readonly("overworld", &yaze_project::overworld);

  class_<yaze_editor_context>("yaze_editor_context")
      .def_readonly("project", &yaze_editor_context::project);

  enum_<yaze_event_type>("yaze_event_type")
      .value("YAZE_EVENT_ROM_LOADED", YAZE_EVENT_ROM_LOADED)
      .value("YAZE_EVENT_ROM_SAVED", YAZE_EVENT_ROM_SAVED)
      .value("YAZE_EVENT_SPRITE_MODIFIED", YAZE_EVENT_SPRITE_MODIFIED)
      .value("YAZE_EVENT_PALETTE_CHANGED", YAZE_EVENT_PALETTE_CHANGED);

  class_<yaze_extension>("yaze_extension")
      .def_readonly("name", &yaze_extension::name)
      .def_readonly("version", &yaze_extension::version);

  // Functions that return raw pointers need to be managed by Python's garbage
  // collector
  def("yaze_load_rom", &yaze_load_rom,
      return_value_policy<manage_new_object>());
  def("yaze_unload_rom", &yaze_unload_rom);  // No need to manage memory here
  def("yaze_get_color_from_paletteset", &yaze_get_color_from_paletteset);
  def("yaze_check_version", &yaze_check_version);
}