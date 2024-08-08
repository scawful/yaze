#ifndef EXTENSION_INTERFACE_H
#define EXTENSION_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yaze_extension {
  const char* name;
  const char* version;

  // Initialization function
  void (*initialize)(void);

  // Cleanup function
  void (*cleanup)(void);

  // Function to extend editor functionality
  void (*extendFunctionality)(void* editorContext);
} yaze_extension;

// Function to get the extension instance
yaze_extension* GetExtension();

void LoadCExtension(const char* extension_path);

// Function to load a Python script as an extension
void LoadPythonExtension(const char* script_path);

#ifdef __cplusplus
}
#endif

#endif  // EXTENSION_INTERFACE_H