load("//bazel:yaze.bzl", "COPTS", "LOPTS")

cc_binary(
    name = "yaze",
    srcs = [
        "yaze.cc",
    ],
    copts = COPTS,
    linkopts = LOPTS,
    deps = [
        "//src:imgui",
        "//src/app/core",
        "//src/app/editor:yaze_app_editor",
        "//src/app/emu:yaze_app_emu",
        "//src/app/gfx:yaze_app_gfx",
        "//src/app/gui:yaze_gui",
        "//src/app/zelda3",
        "@abseil-cpp//absl/status:status",
        "@abseil-cpp//absl/strings",
    ],
)
