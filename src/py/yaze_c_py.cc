#include <Python.h>

static PyObject *yaze_init(PyObject *self, PyObject *args);

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