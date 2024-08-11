#include <boost/python.hpp>

#include "base/snes_color.h"
#include "base/sprite.h"
#include "yaze.h"

BOOST_PYTHON_MODULE(yaze) {
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

  class_<z3_sprite_action>("z3_sprite_action")
      .def_readonly("name", &z3_sprite_action::name)
      .def_readonly("id", &z3_sprite_action::id);

  class_<z3_sprite>("sprite")
      .def_readonly("name", &z3_sprite::name)
      .def_readonly("id", &z3_sprite::id)
      .def_readonly("actions", &z3_sprite::actions);
}
