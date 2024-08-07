#include "yaze_python.h"

#include <Python.h>

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