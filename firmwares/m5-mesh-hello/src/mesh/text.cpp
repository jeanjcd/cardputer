// SPDX-License-Identifier: GPL-3.0-or-later
#include "text.h"
#include <stdint.h>
#include <string.h>

namespace {
// CP437 glyph for U+00A0..U+00FF; 0 = no glyph ('?').
const uint8_t kLatin1[96] = {
    // A0..AF: nbsp ¡ ¢ £ ¤ ¥ ¦ § ¨ © ª « ¬ shy ® ¯
    0x20, 0xAD, 0x9B, 0x9C, 0, 0x9D, 0, 0, 0, 0, 0xA6, 0xAE, 0xAA, 0, 0, 0,
    // B0..BF: ° ± ² ³ ´ µ ¶ · ¸ ¹ º » ¼ ½ ¾ ¿
    0xF8, 0xF1, 0xFD, 0, 0, 0xE6, 0, 0xFA, 0, 0, 0xA7, 0xAF, 0xAC, 0xAB, 0, 0xA8,
    // C0..CF: À Á Â Ã Ä Å Æ Ç È É Ê Ë Ì Í Î Ï
    'A', 'A', 'A', 'A', 0x8E, 0x8F, 0x92, 0x80, 'E', 0x90, 'E', 'E', 'I', 'I', 'I', 'I',
    // D0..DF: Ð Ñ Ò Ó Ô Õ Ö × Ø Ù Ú Û Ü Ý Þ ß
    'D', 0xA5, 'O', 'O', 'O', 'O', 0x99, 'x', 'O', 'U', 'U', 'U', 0x9A, 'Y', 0, 0xE1,
    // E0..EF: à á â ã ä å æ ç è é ê ë ì í î ï
    0x85, 0xA0, 0x83, 'a', 0x84, 0x86, 0x91, 0x87, 0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B,
    // F0..FF: ð ñ ò ó ô õ ö ÷ ø ù ú û ü ý þ ÿ
    'd', 0xA4, 0x95, 0xA2, 0x93, 'o', 0x94, 0xF6, 'o', 0x97, 0xA3, 0x96, 0x81, 'y', 0, 0x98,
};

// Decodes one UTF-8 character at s; returns its byte length (>=1) and sets cp (0xFFFD if invalid).
size_t decode(const uint8_t* s, uint32_t& cp) {
  uint8_t c = s[0];
  if (c < 0x80) { cp = c; return 1; }
  size_t need;
  uint32_t min;
  if ((c & 0xE0) == 0xC0) { need = 2; cp = c & 0x1F; min = 0x80; }
  else if ((c & 0xF0) == 0xE0) { need = 3; cp = c & 0x0F; min = 0x800; }
  else if ((c & 0xF8) == 0xF0) { need = 4; cp = c & 0x07; min = 0x10000; }
  else { cp = 0xFFFD; return 1; }
  for (size_t i = 1; i < need; i++) {
    if ((s[i] & 0xC0) != 0x80) { cp = 0xFFFD; return 1; }  // also stops at NUL
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { cp = 0xFFFD; return 1; }
  return need;
}

uint8_t glyphFor(uint32_t cp) {
  if (cp >= 0x20 && cp < 0x7F) return (uint8_t)cp;
  if (cp >= 0xA0 && cp <= 0xFF) { uint8_t g = kLatin1[cp - 0xA0]; return g ? g : '?'; }
  if (cp == 0x2018 || cp == 0x2019) return '\'';
  if (cp == 0x201C || cp == 0x201D) return '"';
  if (cp == 0x2013 || cp == 0x2014) return '-';
  return '?';
}

bool invisible(uint32_t cp) { return (cp >= 0xFE00 && cp <= 0xFE0F) || cp == 0x200D || cp == 0x200B; }
}  // namespace

size_t utf8ToCp437(const char* src, char* dst, size_t cap) {
  if (!dst || cap == 0) return 0;
  size_t n = 0;
  const uint8_t* s = (const uint8_t*)(src ? src : "");
  while (*s && n + 1 < cap) {
    uint32_t cp;
    s += decode(s, cp);
    if (invisible(cp)) continue;
    dst[n++] = (char)glyphFor(cp);
  }
  dst[n] = 0;
  return n;
}

void utf8PopBack(char* s) {
  if (!s) return;
  size_t n = strlen(s);
  if (n == 0) return;
  size_t i = n - 1;
  size_t back = 0;
  while (i > 0 && back < 3 && ((uint8_t)s[i] & 0xC0) == 0x80) { i--; back++; }
  if (((uint8_t)s[i] & 0xC0) != 0xC0) i = n - 1;  // not a lead byte: drop one stray byte only
  s[i] = 0;
}

bool textAppendAscii(char* s, size_t cap, size_t maxBytes, char c) {
  if (!s || cap == 0 || c < 0x20 || c > 0x7E) return false;
  size_t n = strnlen(s, cap);
  if (n >= cap) return false;
  if (n + 1 > maxBytes || n + 1 > cap - 1) return false;
  s[n] = c;
  s[n + 1] = 0;
  return true;
}

bool utf8CleanText(const char* s, size_t maxBytes) {
  if (!s) return false;
  size_t len = strnlen(s, maxBytes + 1);
  if (len == 0 || len > maxBytes) return false;
  const uint8_t* p = (const uint8_t*)s;
  size_t i = 0;
  while (i < len) {
    uint32_t cp;
    size_t k = decode(p + i, cp);
    if (cp == 0xFFFD || i + k > len) return false;
    if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp < 0xA0)) return false;
    i += k;
  }
  return true;
}

void utf8Copy(char* dst, size_t cap, const char* src) {
  if (!dst || cap == 0) return;
  size_t n = 0;
  if (src) while (n + 1 < cap && src[n]) { dst[n] = src[n]; n++; }
  dst[n] = 0;
  if (src && src[n]) {  // truncated: do not leave a partial character at the end
    size_t i = n, back = 0;
    while (i > 0 && back < 3 && ((uint8_t)dst[i - 1] & 0xC0) == 0x80) { i--; back++; }
    if (i > 0) {
      uint8_t lead = (uint8_t)dst[i - 1];
      size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
      if (lead >= 0xC0 && back + 1 < need) n = i - 1;
    }
    dst[n] = 0;
  }
}
