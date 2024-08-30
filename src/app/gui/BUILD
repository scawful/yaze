load("//bazel:yaze.bzl", "COPTS", "LOPTS")

cc_library(
    name = "yaze_gui",
    srcs = [
        "asset_browser.cc",
        "canvas.cc",
        "color.cc",
        "input.cc",
        "style.cc",
        "zeml.cc",
    ],
    hdrs = [
        "modules/asset_browser.h",
        "canvas.h",
        "color.h",
        "input.h",
        "style.h",
        "zeml.h",
    ],
    copts = COPTS,
    linkopts = LOPTS,
    includes = [
        ".",
        "../../",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/app/core",
        "//src/app/gfx:yaze_app_gfx",
        "@abseil-cpp//absl/strings",
    ],
)
