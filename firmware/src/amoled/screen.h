#pragma once
// Touch UI: RUN view (2x2 pad grid) and a simplified SERVICE editor.
#include "../controller.h"

constexpr uint32_t kLocalOwner = 0x10CA1u;  // session id of the on-screen editor

enum class Hit : uint8_t {
  None, Title, Done, Tab, Freq, LevelBar, Wave, Test, Save
};
struct HitResult { Hit hit = Hit::None; int index = 0; float value = 0; };

struct LocalView {
  bool service = false;  // on-screen editor open
  int pad = 0;           // pad tab being edited
  bool testing = false;
  int pressed = -1;      // RUN: pad under the finger
  const char* toast = nullptr;
};

void screenBegin();
HitResult screenHit(int x, int y, bool service);
// Redraws only when what is shown changed.
void screenUpdate(const ControlSnapshot& s, const LocalView& v, uint32_t now, bool touched);
