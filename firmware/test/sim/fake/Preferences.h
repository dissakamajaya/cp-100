#pragma once
// libraries/Preferences (core 2.0.17) subset, backed by the file named in $CP100_NVS
// so that a second process sees what the first one stored.
#include "Arduino.h"

class Preferences {
 public:
  Preferences() {}
  ~Preferences() { end(); }
  bool begin(const char* name, bool readOnly = false, const char* partition_label = NULL);
  void end();
  bool clear();
  bool remove(const char* key);
  size_t putBytes(const char* key, const void* value, size_t len);
  size_t getBytesLength(const char* key);
  size_t getBytes(const char* key, void* buf, size_t maxLen);
  bool isKey(const char* key);

 private:
  bool started_ = false;
  bool readOnly_ = false;
  char ns_[16] = {0};
};
