#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
// In-memory NVS transport only; the production storage codec is compiled unchanged.
class Preferences {
 public:
  inline static std::map<std::string, std::vector<uint8_t>> data;
  inline static bool failWrites = false, shortRead = false;
  bool begin(const char* name, bool readOnly = false) { ns = std::string(name) + '/'; return !readOnly || !data.empty(); }
  void end() {}
  size_t getBytesLength(const char* key) { auto it = data.find(ns + key); return it == data.end() ? 0 : it->second.size(); }
  size_t getBytes(const char* key, void* out, size_t n) {
    auto it = data.find(ns + key); if (it == data.end() || n < it->second.size()) return 0;
    size_t size = it->second.size(); if (shortRead && size) --size;
    memcpy(out, it->second.data(), size); return size;
  }
  size_t putBytes(const char* key, const void* in, size_t n) {
    if (failWrites) return 0;
    const auto* p = static_cast<const uint8_t*>(in); data[ns + key] = std::vector<uint8_t>(p, p + n); return n;
  }
 private: std::string ns;
};
