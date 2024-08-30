workspace(name = "yet_another_zelda3_editor")

register_toolchains(
    "//bazel:cc_toolchain_for_arm64",
)
new_local_repository(
  name = "sdl2",
  path = "/Library/Frameworks/SDL2.framework",
  build_file = "//src:sdl2",
)
