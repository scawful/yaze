#include "extension.h"

#include <Python.h>
#include <dlfcn.h>

#include <iostream>

void* handle = nullptr;
Extension* extension = nullptr;

Extension* GetExtension() { return nullptr; }

void loadCExtension(const char* extensionPath) {
  handle = dlopen(extensionPath, RTLD_LAZY);
  if (!handle) {
    std::cerr << "Cannot open extension: " << dlerror() << std::endl;
    return;
  }
  dlerror();  // Clear any existing error

  Extension* (*getExtension)();
  getExtension = (Extension * (*)()) dlsym(handle, "getExtension");
  const char* dlsym_error = dlerror();
  if (dlsym_error) {
    std::cerr << "Cannot load symbol 'getExtension': " << dlsym_error
              << std::endl;
    dlclose(handle);
    return;
  }

  extension = getExtension();
  extension->initialize();
}

void LoadPythonExtension(const char* script_path) {
  Py_Initialize();
  FILE* fp = fopen(script_path, "r");
  if (fp) {
    PyRun_SimpleFile(fp, script_path);
    fclose(fp);
  }

  PyObject* pModule = PyImport_AddModule("__main__");
  PyObject* pFunc = PyObject_GetAttrString(pModule, "get_extension");

  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject* pExtension = PyObject_CallObject(pFunc, nullptr);
    if (pExtension) {
      // Assume the Python extension has the same structure as the C extension
      extension = reinterpret_cast<Extension*>(PyLong_AsVoidPtr(pExtension));
    }
  }
}