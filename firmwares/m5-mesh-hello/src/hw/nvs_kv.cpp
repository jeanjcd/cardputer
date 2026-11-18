#include "nvs_kv.h"

#include <string.h>

namespace {
constexpr const char* kNamespace = "meshhello";
constexpr size_t kMaxKeyLen = 15;  // NVS key limit

bool validKey(const char* key) {
  if (!key) return false;
  size_t n = strnlen(key, kMaxKeyLen + 1);
  return n > 0 && n <= kMaxKeyLen;
}
}  // namespace

bool NvsKv::begin() {
  if (!open_) open_ = prefs_.begin(kNamespace, false);
  return open_;
}

bool NvsKv::get(const char* key, void* buf, size_t len) {
  if (!open_ || !buf || len == 0 || !validKey(key)) return false;
  if (!prefs_.isKey(key)) return false;
  // Exact-length contract: a blob of a different size is treated as absent.
  if (prefs_.getBytesLength(key) != len) return false;
  return prefs_.getBytes(key, buf, len) == len;
}

bool NvsKv::put(const char* key, const void* buf, size_t len) {
  if (!open_ || !buf || len == 0 || !validKey(key)) return false;
  return prefs_.putBytes(key, buf, len) == len;
}
