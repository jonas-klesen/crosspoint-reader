#pragma once

#include "activities/Activity.h"
#include "components/UiAppHost.h"

class StudyHomeActivity final : public Activity, private UiAppHost {
 public:
  explicit StudyHomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static void screenTrampoline(UiScreen& screen, void* user);
  void buildScreen(UiScreen& screen);
};
