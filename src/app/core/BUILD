load("//bazel:yaze.bzl", "COPTS", "LOPTS")

cc_library(
    name = "core",
    srcs = [
        "common.cc",
        "controller.cc",
        "labeling.cc",
        "message.cc",
    ],
    hdrs = [
        "common.h",
        "controller.h",
        "labeling.h",
        "message.h",
    ],
    copts = COPTS,
    linkopts = LOPTS,
    includes = [
        ".",
        "../../",
    ],
    visibility = ["//visibility:public"],
)
