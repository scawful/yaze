# SDL2, SDL2_image and SDL2_mixer
if (UNIX)
  add_subdirectory(src/lib/SDL)
else()
  find_package(SDL2)
endif()
set(SDL2MIXER_OPUS OFF)
set(SDL2MIXER_FLAC OFF)
set(SDL2MIXER_MOD OFF)
set(SDL2MIXER_MIDI_FLUIDSYNTH OFF)
find_library(SDL_MIXER_LIBRARY
  NAMES SDL_mixer
  HINTS
    ENV SDLMIXERDIR
    ENV SDLDIR
  PATH_SUFFIXES lib ${VC_LIB_PATH_SUFFIX}
  )
add_subdirectory(src/lib/SDL_mixer)
find_package(SDL2_image)