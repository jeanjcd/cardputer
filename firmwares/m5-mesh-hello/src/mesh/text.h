#pragma once
#include <stddef.h>

// Bounded UTF-8 text helpers for the UI. Display conversion never alters the text that is sent.

/// Converts UTF-8 to one CP437 glyph byte per character (Font0 glyph indices) for DISPLAY ONLY:
/// Latin-1 letters map to their CP437 glyph or base ASCII letter, anything else to '?'; control bytes become '?',
/// variation selectors / zero-width joiners are dropped. Writes at most cap-1 glyphs plus NUL; returns the glyph count.
size_t utf8ToCp437(const char* src, char* dst, size_t cap);

/// Removes the last UTF-8 character (or one stray byte) from the NUL-terminated string s.
void utf8PopBack(char* s);

/// Appends printable ASCII c (0x20..0x7E) if strlen(s)+1 stays within maxBytes and cap-1; returns true if appended.
bool textAppendAscii(char* s, size_t cap, size_t maxBytes, char c);

/// True if s has 1..maxBytes bytes before its NUL (checked without reading past maxBytes+1), is valid UTF-8
/// (no overlongs/surrogates) and contains no control characters.
bool utf8CleanText(const char* s, size_t maxBytes);

/// Copies src into dst[cap] (always NUL-terminated), cutting at a UTF-8 character boundary.
void utf8Copy(char* dst, size_t cap, const char* src);
