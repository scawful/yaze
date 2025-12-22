#ifdef __EMSCRIPTEN__
/**
 * @file wasm_z3ed_stub.cc
 * @brief Stub implementation of GetGlobalEditorManager for z3ed CLI build.
 * 
 * The z3ed CLI tool does not link against the main application controller,
 * so it lacks the definition of GetGlobalEditorManager. This stub satisfies
 * the linker dependency introduced by wasm_terminal_bridge.cc.
 */

namespace yaze::editor {
class EditorManager;
}

namespace yaze::app {

yaze::editor::EditorManager* GetGlobalEditorManager() {
  return nullptr;
}

} // namespace yaze::app

#endif // __EMSCRIPTEN__
