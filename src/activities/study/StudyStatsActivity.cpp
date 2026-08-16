#include "StudyStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "StudyFormat.h"
#include "components/UITheme.h"
#include "fontIds.h"

StudyStatsActivity::StudyStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("StudyStats", renderer, mappedInput) {}

void StudyStatsActivity::onEnter() {
  Activity::onEnter();
  studypet::StudyStatsStore store;
  const studypet::StudyStatsLoadResult loaded = store.load();
  stats = loaded.stats;
  unavailable = !loaded.available();
  requestUpdate();
}

void StudyStatsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) finish();
}

void StudyStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_STUDY_STATS_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = height - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int spacing = metrics.verticalSpacing;

  if (unavailable) {
    const int blockHeight = lineHeight * 2 + spacing;
    int y = contentTop + std::max(0, (contentBottom - contentTop - blockHeight) / 2);
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_STUDY_STATS_UNAVAILABLE), true, EpdFontFamily::BOLD);
    y += lineHeight + spacing;
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_STUDY_STATS_UNAVAILABLE_HINT));
  } else if (stats.totalReviews == 0) {
    const int blockHeight = lineHeight * 2 + spacing;
    int y = contentTop + std::max(0, (contentBottom - contentTop - blockHeight) / 2);
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_STUDY_STATS_EMPTY), true, EpdFontFamily::BOLD);
    y += lineHeight + spacing;
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_STUDY_STATS_EMPTY_HINT));
  } else {
    const int blockHeight = lineHeight * 5 + spacing * 2;
    int y = contentTop + std::max(0, (contentBottom - contentTop - blockHeight) / 2);
    char line[96]{};

    const auto drawCounter = [&](const char* format, const unsigned long value) {
      studypet::safeFormat(line, sizeof(line), tr(STR_STUDY_INVALID), format, value);
      renderer.drawCenteredText(UI_10_FONT_ID, y, line);
      y += lineHeight;
    };
    drawCounter(tr(STR_STUDY_STATS_REVIEWED_FORMAT), static_cast<unsigned long>(stats.totalReviews));
    drawCounter(tr(STR_STUDY_STATS_KNOWN_FORMAT), static_cast<unsigned long>(stats.known));
    drawCounter(tr(STR_STUDY_STATS_DID_NOT_KNOW_FORMAT), static_cast<unsigned long>(stats.didNotKnow));
    y += spacing;

    studypet::safeFormat(line, sizeof(line), tr(STR_STUDY_INVALID), tr(STR_STUDY_STATS_PERCENT_FORMAT),
                         static_cast<unsigned int>(studycore::knownPercentage(stats)));
    renderer.drawCenteredText(UI_10_FONT_ID, y, line);
    y += lineHeight + spacing;
    drawCounter(tr(STR_STUDY_STATS_SESSIONS_FORMAT), static_cast<unsigned long>(stats.completedSessions));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
