# SDL2, SDL2_image
if (UNIX)
  add_subdirectory(src/lib/SDL)
else()
  find_package(SDL2)
endif()
find_package(SDL2_image)