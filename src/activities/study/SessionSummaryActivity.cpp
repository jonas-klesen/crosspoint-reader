#include "SessionSummaryActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <utility>

#include "MappedInputManager.h"
#include "StudyFormat.h"
#include "components/UITheme.h"
#include "fontIds.h"

SessionSummaryActivity::SessionSummaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               std::string deckDisplayName, const std::size_t reviewedCards)
    : Activity("StudySessionSummary", renderer, mappedInput),
      deckDisplayName(std::move(deckDisplayName)),
      reviewedCards(reviewedCards) {}

void SessionSummaryActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void SessionSummaryActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void SessionSummaryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, deckDisplayName.c_str());

  char count[64]{};
  studypet::safeFormat(count, sizeof(count), tr(STR_STUDY_INVALID), tr(STR_STUDY_REVIEWED_COUNT_FORMAT), reviewedCards);
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = height - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int y = contentTop + std::max(0, (contentBottom - contentTop - 2 * lineHeight) / 2);
  renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_STUDY_SESSION_COMPLETE), true);
  renderer.drawCenteredText(UI_12_FONT_ID, y + lineHeight, count, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DONE), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
