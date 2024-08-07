#include "yaze_py.h"

#include <Python.h>

#include <boost/python.hpp>

#include "yaze.h"

BOOST_PYTHON_MODULE(yaze) {
  using namespace boost::python;
  def("yaze_init", yaze_init);

  class_<Rom>("Rom")
      .def_readonly("filename", &Rom::filename)
      .def_readonly("data", &Rom::data)
      .def_readonly("size", &Rom::size)
      .def_readonly("impl", &Rom::impl);
}

static PyObject *SpamError;

static PyObject *yaze_init(PyObject *self, PyObject *args) {
  const char *command;
  int sts;

  if (!PyArg_ParseTuple(args, "s", &command)) return NULL;
  sts = system(command);
  if (sts < 0) {
    PyErr_SetString(SpamError, "System command failed");
    return NULL;
  }
  return PyLong_FromLong(sts);
}