class Z3ed < Formula
  desc "AI-powered CLI tool for Zelda: A Link to the Past ROM hacking"
  homepage "https://github.com/scawful/yaze"
  url "https://github.com/scawful/yaze/archive/refs/tags/v0.5.6.tar.gz"
  sha256 ""  # TODO: fill after release is published
  license "GPL-3.0-or-later"
  head "https://github.com/scawful/yaze.git", branch: "master"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build

  depends_on "abseil"
  depends_on "libpng"

  def install
    args = %w[
      -DYAZE_BUILD_APP=OFF
      -DYAZE_BUILD_CLI=ON
      -DYAZE_BUILD_Z3ED=ON
      -DYAZE_BUILD_TESTS=OFF
      -DYAZE_ENABLE_AI=OFF
      -DYAZE_ENABLE_AI_RUNTIME=OFF
      -DYAZE_ENABLE_GRPC=OFF
      -DYAZE_ENABLE_JSON=ON
      -DYAZE_MINIMAL_BUILD=ON
      -DCMAKE_BUILD_TYPE=Release
    ]

    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja", *args, *std_cmake_args
    system "cmake", "--build", "build"

    bin.install "build/bin/Release/z3ed" if File.exist?("build/bin/Release/z3ed")
    bin.install "build/bin/z3ed" if File.exist?("build/bin/z3ed")
  end

  test do
    assert_predicate bin/"z3ed", :exist?
    system bin/"z3ed", "--help"
  end
end
