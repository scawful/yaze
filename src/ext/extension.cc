#include "extension.h"

#include <Python.h>
#include <dlfcn.h>

#include <iostream>

static void* handle = nullptr;
static yaze_extension* extension = nullptr;

yaze_extension* get_yaze_extension() { return extension; }

void yaze_load_c_extension(const char* extension_path) {
  handle = dlopen(extension_path, RTLD_LAZY);
  if (!handle) {
    std::cerr << "Cannot open extension: " << dlerror() << std::endl;
    return;
  }
  dlerror();  // Clear any existing error

  // Load the symbol to retrieve the extension
  auto get_extension = reinterpret_cast<yaze_extension* (*)()>(
      dlsym(handle, "get_yaze_extension"));
  const char* dlsym_error = dlerror();
  if (dlsym_error) {
    std::cerr << "Cannot load symbol 'get_yaze_extension': " << dlsym_error
              << std::endl;
    dlclose(handle);
    return;
  }

  extension = get_extension();
  if (extension && extension->initialize) {
    extension->initialize();
  } else {
    std::cerr << "Failed to initialize the extension." << std::endl;
    dlclose(handle);
    handle = nullptr;
    extension = nullptr;
  }
}

void yaze_load_py_extension(const char* script_path) {
    Py_Initialize();

    FILE* fp = fopen(script_path, "r");
    if (fp) {
        PyRun_SimpleFile(fp, script_path);
        fclose(fp);
    } else {
        std::cerr << "Cannot open Python script: " << script_path << std::endl;
        return;
    }

    PyObject* pModule = PyImport_AddModule("__main__");
    PyObject* pFunc = PyObject_GetAttrString(pModule, "get_yaze_extension");

    if (pFunc && PyCallable_Check(pFunc)) {
        PyObject* pExtension = PyObject_CallObject(pFunc, nullptr);
        if (pExtension) {
            extension = reinterpret_cast<yaze_extension*>(PyLong_AsVoidPtr(pExtension));
            if (extension && extension->initialize) {
                extension->initialize();
            }
        } else {
            std::cerr << "Failed to get extension from Python script." << std::endl;
        }
        Py_XDECREF(pExtension);
    } else {
        std::cerr << "Python function 'get_yaze_extension' not found or not callable." << std::endl;
    }

    Py_XDECREF(pFunc);
    Py_Finalize();
}

void yaze_cleanup_extension() {
  if (extension && extension->cleanup) {
    extension->cleanup();
  }
  if (handle) {
    dlclose(handle);
    handle = nullptr;
    extension = nullptr;
  }
}