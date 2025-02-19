#include <boost/python.hpp>

#include "incl/extension.h"
#include "incl/overworld.h"
#include "incl/snes_color.h"
#include "yaze.h"

BOOST_PYTHON_MODULE(yaze_py) {
  using namespace boost::python;

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

  class_<yaze_flags>("yaze_flags")
      .def_readwrite("debug", &yaze_flags::debug)
      .def_readwrite("rom_filename", &yaze_flags::rom_filename)
      .def_readwrite("rom", &yaze_flags::rom);

  class_<yaze_project>("yaze_project")
    .def_readonly("filename", &yaze_project::filepath);

  class_<yaze_editor_context>("yaze_editor_context")
      .def_readonly("project", &yaze_editor_context::project);

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
