load("//bazel:yaze.bzl", "COPTS", "LOPTS")

cc_library(
    name = "zelda3",
    srcs = [
        "overworld/overworld.cc",
        "overworld/overworld_map.cc",
    ],
    hdrs = [
        "overworld/overworld.h",
        "overworld/overworld_map.h",
    ],
    copts = COPTS,
    linkopts = LOPTS,
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/status:status",
    ],
)
