#include "StudyHomeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "DeckListActivity.h"
#include "MappedInputManager.h"
#include "StudyFormat.h"
#include "StudyStatsActivity.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

StudyHomeActivity::StudyHomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("StudyHome", renderer, mappedInput) {}

const char* StudyHomeActivity::headerTitle() const { return tr(STR_STUDY_PET); }

void StudyHomeActivity::onEnter() {
  UiListActivity::onEnter();
  rowItems = {};
  rowItems[0].label = tr(STR_STUDY_DECKS);
  rowItems[0].subtitle = tr(STR_STUDY_DECKS_SUBTITLE);
  rowItems[0].actionValue = 0;
  rowItems[1].label = tr(STR_STUDY_STATS);
  rowItems[1].subtitle = tr(STR_STUDY_STATS_SUBTITLE);
  rowItems[1].actionValue = 1;
}

void StudyHomeActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMargin(fui::Insets{studypet::clampedInset(safe.y + metrics.topPadding + metrics.headerHeight),
                                      studypet::clampedInset(renderer.getScreenWidth() - (safe.x + safe.width)),
                                      studypet::clampedInset(renderer.getScreenHeight() - (safe.y + safe.height)),
                                      studypet::clampedInset(safe.x)});
  screen.spacer(studypet::clampedInset(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = rowItems.size();
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props, true);
  screen.list(props);
}

void StudyHomeActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();

  if (index == 0) {
    auto deckList = makeUniqueNoThrow<DeckListActivity>(renderer, mappedInput);
    if (!deckList) {
      LOG_ERR("Study", "OOM: deck list activity");
      return;
    }
    activityManager.pushActivity(std::move(deckList));
    return;
  }

  auto stats = makeUniqueNoThrow<StudyStatsActivity>(renderer, mappedInput);
  if (!stats) {
    LOG_ERR("Study", "OOM: stats activity");
    return;
  }
  activityManager.pushActivity(std::move(stats));
}
