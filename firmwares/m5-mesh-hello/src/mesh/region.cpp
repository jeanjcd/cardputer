#include "mesh/region.h"
#include <math.h>

const uint8_t kDefaultKey[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
                                 0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01};

uint32_t djb2(const char* s) {
  uint32_t h = 5381;
  while (*s) h = ((h << 5) + h) + (uint8_t)*s++;
  return h;
}

float regionFreqMHz(Region region, const char* channelName, float bwKHz) {
  float start, end;
  switch (region) {
    case Region::BR_902: start = 902.0f; end = 907.5f; break;
    case Region::ANZ:
    default: start = 915.0f; end = 928.0f; break;
  }
  float bw = bwKHz / 1000.0f;
  uint32_t numChannels = (uint32_t)floorf((end - start) / bw);
  uint32_t slot = djb2(channelName) % numChannels;
  return start + bw / 2.0f + slot * bw;
}

uint8_t channelHash(const char* name, const uint8_t* key, size_t keyLen) {
  uint8_t h = 0;
  while (*name) h ^= (uint8_t)*name++;
  for (size_t i = 0; i < keyLen; i++) h ^= key[i];
  return h;
}
