#include "StudyHomeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "DeckListActivity.h"
#include "MappedInputManager.h"
#include "StudyFormat.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

StudyHomeActivity::StudyHomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("StudyHome", renderer, mappedInput) {}

const char* StudyHomeActivity::headerTitle() const { return tr(STR_STUDY_PET); }

void StudyHomeActivity::onEnter() {
  UiListActivity::onEnter();
  rowItem = {};
  rowItem.label = tr(STR_STUDY_DECKS);
  rowItem.subtitle = tr(STR_STUDY_DECKS_SUBTITLE);
  rowItem.actionValue = 0;
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
  props.items = &rowItem;
  props.count = 1;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props, true);
  screen.list(props);
}

void StudyHomeActivity::activateIndex(const int index) {
  if (index != 0) return;
  app.clearTapFlash();
  auto deckList = makeUniqueNoThrow<DeckListActivity>(renderer, mappedInput);
  if (!deckList) {
    LOG_ERR("Study", "OOM: deck list activity");
    return;
  }
  activityManager.pushActivity(std::move(deckList));
}
