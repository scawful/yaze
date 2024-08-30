# load(":cc_toolchain_config.bzl", "cc_toolchain_config")
package(default_visibility = ["//visibility:public"])


# cc_toolchain_config(name = "brew_clang_toolchain")

filegroup(name = "empty")

cc_toolchain(
  name = "arm64_toolchain",
  toolchain_identifier = "arm64-toolchain",
  toolchain_config = ":arm64_toolchain_config",
  all_files = ":empty",
  compiler_files = ":empty",
  dwp_files = ":empty",
  linker_files = ":empty",
  objcopy_files = ":empty",
  strip_files = ":empty",
  supports_param_files = 0,
)

toolchain(
  name = "cc_toolchain_for_arm64",
  toolchain = ":arm64_toolchain",
  toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
  exec_compatible_with = [
    "@platforms//cpu:arm64",
    "@platforms//os:linux",
  ],
  target_compatible_with = [
    "@platforms//cpu:arm64",
    "@platforms//os:linux",
  ],
)
