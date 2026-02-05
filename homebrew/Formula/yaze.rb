class Yaze < Formula
  desc "Yet Another Zelda3 Editor - ROM editor for Zelda: A Link to the Past"
  homepage "https://github.com/scawful/yaze"
  url "https://github.com/scawful/yaze/archive/refs/tags/v0.5.6.tar.gz"
  sha256 ""  # TODO: fill after release is published
  license "GPL-3.0-or-later"
  head "https://github.com/scawful/yaze.git", branch: "master"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build

  depends_on "abseil"
  depends_on "sdl2"
  depends_on "libpng"

  on_macos do
    depends_on "grpc"
    depends_on "protobuf"
  end

  on_linux do
    depends_on "mesa"
    depends_on "libx11"
    depends_on "libxrandr"
  end

  def install
    args = %w[
      -DYAZE_BUILD_GUI=ON
      -DYAZE_BUILD_CLI=ON
      -DYAZE_BUILD_TESTS=OFF
      -DYAZE_ENABLE_AI=OFF
      -DYAZE_ENABLE_AI_RUNTIME=OFF
      -DYAZE_ENABLE_GRPC=OFF
      -DYAZE_ENABLE_JSON=ON
      -DCMAKE_BUILD_TYPE=Release
    ]

    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja", *args, *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build", "--prefix", prefix
  end

  test do
    assert_predicate bin/"yaze", :exist?
    system bin/"yaze", "--version"
  end
end
