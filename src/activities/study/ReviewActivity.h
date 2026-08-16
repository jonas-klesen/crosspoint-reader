#pragma once

#include <cstddef>
#include <string>

#include "SessionStats.h"
#include "StudyStats.h"
#include "activities/Activity.h"
#include "study/storage/DeckRepository.h"

class ReviewActivity final : public Activity {
 public:
  ReviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, studycore::Deck deck,
                 std::string deckDisplayName);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class ReviewPhase { Front, Revealed };

  void revealCard();
  void judgeCard(studycore::RecallJudgment judgment);
  void showCompletion();
  void commitSessionStats(bool completed);
  void drawTextBlock(const std::string& text, int top, int bottom, int maxLines, int fontId) const;

  studycore::Deck deck;
  std::string deckDisplayName;
  std::size_t currentCardIndex = 0;
  ReviewPhase phase = ReviewPhase::Front;
  studycore::SessionStats sessionStats{};
  studycore::SessionStatsCommitGuard statsCommitGuard{};
};
