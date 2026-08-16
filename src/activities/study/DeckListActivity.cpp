#include "DeckListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include <cstdio>
#include <memory>

#include "MappedInputManager.h"
#include "StudyDeckDetailsActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

DeckListActivity::DeckListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("DeckList", renderer, mappedInput) {}

const char* DeckListActivity::headerTitle() const { return tr(STR_STUDY_DECKS); }

void DeckListActivity::onEnter() {
  UiListActivity::onEnter();
  reloadDecks();
}

void DeckListActivity::reloadDecks() {
  closeRouting();
  scanResult = repository.listDecks();
  nav.reset();
  rebuildRows();
}

void DeckListActivity::rebuildRows() {
  rowLabels.clear();
  rowValues.clear();
  rowItems.clear();
  rowLabels.reserve(scanResult.decks.size());
  rowValues.reserve(scanResult.decks.size());
  rowItems.reserve(scanResult.decks.size());

  for (std::size_t i = 0; i < scanResult.decks.size(); ++i) {
    const auto& descriptor = scanResult.decks[i];
    rowLabels.push_back(descriptor.displayName);
    if (descriptor.status == studypet::DeckStatus::Valid) {
      char value[32]{};
      std::snprintf(value, sizeof(value), tr(STR_STUDY_CARD_COUNT_FORMAT), descriptor.cardCount);
      rowValues.emplace_back(value);
    } else if (descriptor.status == studypet::DeckStatus::Invalid) {
      rowValues.emplace_back(tr(STR_STUDY_INVALID));
    } else {
      rowValues.emplace_back(tr(STR_STUDY_UNREADABLE));
    }

    fui::ListItem item;
    item.label = rowLabels.back().c_str();
    item.value = rowValues.back().c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems.push_back(item);
  }
}

void DeckListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                                      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                                      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)),
                                      static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (scanResult.decks.empty()) {
    const char* status = scanResult.error == studypet::DeckRepositoryError::StorageUnavailable
                             ? tr(STR_STUDY_STORAGE_UNAVAILABLE)
                             : tr(STR_STUDY_NO_DECKS);
    const auto lineHeight = screen.target().lineHeight(screen.theme().bodyText.font);
    const fui::Rect body = screen.body().inset(fui::Insets{0, 8, 0, 8});
    screen.target().text(fui::Rect{body.x, body.y, body.width, lineHeight}, status, screen.theme().bodyText);
    screen.target().text(fui::Rect{body.x, static_cast<int16_t>(body.y + lineHeight), body.width, lineHeight},
                         tr(STR_STUDY_DECK_PATH), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

void DeckListActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(scanResult.decks.size())) return;
  app.clearTapFlash();

  auto descriptor = scanResult.decks[static_cast<std::size_t>(index)];
  if (descriptor.status == studypet::DeckStatus::Valid) {
    auto loaded = repository.loadDeck(static_cast<std::size_t>(index));
    if (loaded.ok()) {
      descriptor.cardCount = loaded.deck.cards.size();
    } else {
      descriptor.status = loaded.parseError.code == studycore::DeckParseErrorCode::None
                              ? studypet::DeckStatus::Unreadable
                              : studypet::DeckStatus::Invalid;
      descriptor.repositoryError = loaded.error;
      descriptor.parseError = loaded.parseError;
    }
  }

  auto details = makeUniqueNoThrow<StudyDeckDetailsActivity>(renderer, mappedInput, std::move(descriptor));
  if (!details) {
    LOG_ERR("Study", "OOM: deck details activity");
    return;
  }
  startActivityForResult(std::move(details), [this](const ActivityResult&) { reloadDecks(); });
}
