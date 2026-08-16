#pragma once

#include "activities/Activity.h"
#include "study/storage/StudyStatsStore.h"

class StudyStatsActivity final : public Activity {
 public:
  StudyStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  studycore::StudyStats stats{};
  bool unavailable = false;
};
