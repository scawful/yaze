#include <Python.h>

#include <boost/python.hpp>

#include "base/snes_color.h"
#include "base/sprite.h"
#include "yaze.h"

static PyObject *yaze_init(PyObject *self, PyObject *args);

BOOST_PYTHON_MODULE(yaze) {
  using namespace boost::python;
  def("yaze_init", yaze_init);

  class_<rom>("rom")
      .def_readonly("filename", &rom::filename)
      .def_readonly("data", &rom::data)
      .def_readonly("size", &rom::size)
      .def_readonly("impl", &rom::impl);

  class_<snes_color>("snes_color")
      .def_readonly("red", &snes_color::red)
      .def_readonly("green", &snes_color::green)
      .def_readonly("blue", &snes_color::blue);

  class_<snes_palette>("snes_palette")
      .def_readonly("id", &snes_palette::id)
      .def_readonly("size", &snes_palette::size)
      .def_readonly("colors", &snes_palette::colors);

  class_<sprite_action>("sprite_action")
      .def_readonly("name", &sprite_action::name)
      .def_readonly("id", &sprite_action::id);

  class_<sprite>("sprite")
      .def_readonly("name", &sprite::name)
      .def_readonly("id", &sprite::id)
      .def_readonly("actions", &sprite::actions);
}

/**
 * Python C API Example, in case I need more functionality than Boost.Python
 */
static PyMethodDef YazeMethods[] = {
    {"system", yaze_init, METH_VARARGS, "Initialize the yaze lib."},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

static struct PyModuleDef yaze_module = {PyModuleDef_HEAD_INIT,
                                         "yaze",  // Module title
                                         NULL,    // Documentation
                                         -1,      // Interpreter state size
                                         YazeMethods};

// PyMODINIT_FUNC PyInit_yaze(void) { return PyModule_Create(&yaze_module); }

static PyObject *YazeError;

static PyObject *yaze_init(PyObject *self, PyObject *args) {
  const char *command;
  int sts;

  if (!PyArg_ParseTuple(args, "s", &command)) return NULL;
  sts = system(command);
  if (sts < 0) {
    PyErr_SetString(YazeError, "System command failed");
    return NULL;
  }
  return PyLong_FromLong(sts);
}