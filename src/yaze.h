#ifndef YAZE_H
#define YAZE_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

void yaze_initialize(void);

void yaze_cleanup(void);

typedef struct Rom Rom;

struct Rom {
  const char* filename;
  const uint8_t* data;
};

Rom load_rom(const char* filename);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_H
