#pragma once

#include <cstddef>
#include <string>

#include "SessionStats.h"
#include "activities/Activity.h"
class SessionSummaryActivity final : public Activity {
 public:
  SessionSummaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string deckDisplayName,
                         studycore::SessionStats sessionStats);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string deckDisplayName;
  studycore::SessionStats sessionStats{};
};
