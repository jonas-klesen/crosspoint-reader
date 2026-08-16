#include "ReviewActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <utility>

#include "MappedInputManager.h"
#include "SessionSummaryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int CARD_SIDE_PADDING = 20;
constexpr int CARD_TEXT_FONT = NOTOSERIF_18_FONT_ID;
constexpr int CARD_FALLBACK_FONT = NOTOSERIF_14_FONT_ID;

int maxLinesFor(const int height, const int lineHeight) { return std::max(1, height / std::max(1, lineHeight)); }
}  // namespace

ReviewActivity::ReviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, studycore::Deck deck,
                               std::string deckDisplayName)
    : Activity("StudyReview", renderer, mappedInput),
      deck(std::move(deck)),
      deckDisplayName(std::move(deckDisplayName)) {}

void ReviewActivity::onEnter() {
  Activity::onEnter();
  currentCardIndex = 0;
  phase = ReviewPhase::Front;
  LOG_INF("Study", "Started review: %s, %zu cards", deckDisplayName.c_str(), deck.cards.size());
  requestUpdate();
}

void ReviewActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    LOG_DBG("Study", "Review exited early at card %zu/%zu", currentCardIndex + 1, deck.cards.size());
    finish();
    return;
  }

  if (!mappedInput.wasReleased(MappedInputManager::Button::Confirm)) return;

  if (phase == ReviewPhase::Front) {
    revealCard();
  } else {
    advanceCard();
  }
}

void ReviewActivity::revealCard() {
  phase = ReviewPhase::Revealed;
  requestUpdate();
}

void ReviewActivity::advanceCard() {
  ++currentCardIndex;
  if (currentCardIndex >= deck.cards.size()) {
    showCompletion();
    return;
  }

  phase = ReviewPhase::Front;
  requestUpdate();
}

void ReviewActivity::showCompletion() {
  LOG_INF("Study", "Review completed: %zu cards", deck.cards.size());
  auto summary = makeUniqueNoThrow<SessionSummaryActivity>(renderer, mappedInput, deckDisplayName, deck.cards.size());
  if (!summary) {
    LOG_ERR("Study", "OOM: session summary activity");
    finish();
    return;
  }
  startActivityForResult(std::move(summary), [this](const ActivityResult&) { finish(); });
}

void ReviewActivity::drawTextBlock(const std::string& text, const int top, const int bottom, const int maxLines,
                                   const int fontId) const {
  const int width = renderer.getScreenWidth() - 2 * CARD_SIDE_PADDING;
  const int lineHeight = renderer.getLineHeight(fontId);
  if (width <= 0 || bottom <= top || lineHeight <= 0) return;

  auto lines = renderer.wrappedText(fontId, text.c_str(), width, maxLines);
  if (lines.empty()) return;

  const int blockHeight = lineHeight * static_cast<int>(lines.size());
  int y = top + std::max(0, (bottom - top - blockHeight) / 2);
  for (const auto& line : lines) {
    renderer.drawCenteredText(fontId, y, line.c_str());
    y += lineHeight;
  }
}

void ReviewActivity::render(RenderLock&&) {
  if (currentCardIndex >= deck.cards.size()) return;

  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  const int headerY = metrics.topPadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = height - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const auto& card = deck.cards[currentCardIndex];

  GUI.drawHeader(renderer, Rect{0, headerY, width, metrics.headerHeight}, deckDisplayName.c_str());

  char progress[32]{};
  std::snprintf(progress, sizeof(progress), tr(STR_STUDY_PROGRESS_FORMAT), currentCardIndex + 1, deck.cards.size());
  const int progressWidth = renderer.getTextWidth(UI_10_FONT_ID, progress);
  renderer.drawText(UI_10_FONT_ID, width - CARD_SIDE_PADDING - progressWidth,
                    headerY + (metrics.headerHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2, progress);

  const int separatorY = contentTop + (contentBottom - contentTop) * 2 / 5;
  const int frontBottom = phase == ReviewPhase::Front ? contentBottom : separatorY - metrics.verticalSpacing;
  drawTextBlock(card.front, contentTop, frontBottom,
                maxLinesFor(frontBottom - contentTop, renderer.getLineHeight(CARD_TEXT_FONT)), CARD_TEXT_FONT);

  if (phase == ReviewPhase::Revealed) {
    renderer.drawLine(CARD_SIDE_PADDING, separatorY, width - CARD_SIDE_PADDING, separatorY, true);
    drawTextBlock(
        card.back, separatorY + metrics.verticalSpacing, contentBottom,
        maxLinesFor(contentBottom - separatorY - metrics.verticalSpacing, renderer.getLineHeight(CARD_FALLBACK_FONT)),
        CARD_FALLBACK_FONT);
  }

  const auto labels = mappedInput.mapLabels(
      tr(STR_BACK), phase == ReviewPhase::Front ? tr(STR_STUDY_REVIEW_FRONT) : tr(STR_STUDY_REVIEW_NEXT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
