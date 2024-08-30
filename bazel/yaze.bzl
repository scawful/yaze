""""Yaze build rules."""

COPTS = [
    "-std=gnu++20",
    "-arch arm64",
    "-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.5.sdk",
    "-Ilib/SDL/include",
]

LOPTS = [
    "-F/Library/Frameworks",
    "-framework SDL2",
]