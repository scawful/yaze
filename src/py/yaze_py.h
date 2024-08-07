#ifndef YAZE_PYTHON_H
#define YAZE_PYTHON_H

#include <Python.h>

static PyObject *yaze_init(PyObject *self, PyObject *args);

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

#endif  // YAZE_PYTHON_H