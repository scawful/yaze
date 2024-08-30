load("//bazel:yaze.bzl", "COPTS")

cc_library(
    name = "yaze_app_gfx",
    srcs = [
        "bitmap.cc",
        "compression.cc",
        "scad_format.cc",
        "snes_color.cc",
        "snes_palette.cc",
        "snes_tile.cc",
        "tilesheet.cc",
    ],
    hdrs = [
        "bitmap.h",
        "compression.h",
        "scad_format.h",
        "snes_color.h",
        "snes_palette.h",
        "snes_tile.h",
        "tilesheet.h",
    ],
    copts = COPTS,
    includes = ["../../"],
    visibility = ["//visibility:public"],
)
