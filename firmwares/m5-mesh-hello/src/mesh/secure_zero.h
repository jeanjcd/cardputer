// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>

/// Zeroes n bytes at p through a volatile pointer so the compiler cannot elide it.
inline void secureZero(void* p, size_t n) {
  volatile unsigned char* v = static_cast<volatile unsigned char*>(p);
  while (n--) *v++ = 0;
}
