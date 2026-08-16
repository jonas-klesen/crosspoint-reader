#pragma once

#include <cstddef>
#include <string>

#include "activities/Activity.h"

class SessionSummaryActivity final : public Activity {
 public:
  SessionSummaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string deckDisplayName,
                         std::size_t reviewedCards);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::string deckDisplayName;
  std::size_t reviewedCards;
};
