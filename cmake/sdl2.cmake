# SDL2
if (UNIX)
  add_subdirectory(src/lib/SDL)
else()
  find_package(SDL2)
endif()