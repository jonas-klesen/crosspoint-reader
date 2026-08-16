#include "DeckListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdint>
#include <limits>
#include <memory>

#include "MappedInputManager.h"
#include "StudyDeckDetailsActivity.h"
#include "StudyFormat.h"
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

  // Rows carry an int16 action value; once the deck count exceeds that range
  // every further row would wrap, so stop listing there (and log once) rather
  // than showing corrupted selections.
  const std::size_t rowLimit =
      std::min(scanResult.decks.size(), static_cast<std::size_t>(std::numeric_limits<int16_t>::max()) + 1);
  if (scanResult.decks.size() > rowLimit) {
    LOG_ERR("Study", "Deck list has %zu entries; showing the first %zu", scanResult.decks.size(), rowLimit);
  }
  rowLabels.reserve(rowLimit);
  rowValues.reserve(rowLimit);
  rowItems.reserve(rowLimit);

  for (std::size_t i = 0; i < rowLimit; ++i) {
    const auto& descriptor = scanResult.decks[i];
    rowLabels.push_back(descriptor.displayName);
    if (descriptor.status == studypet::DeckStatus::Valid) {
      char value[32]{};
      studypet::safeFormat(value, sizeof(value), tr(STR_STUDY_INVALID), tr(STR_STUDY_CARD_COUNT_FORMAT),
                           descriptor.cardCount);
      rowValues.emplace_back(value);
    } else if (descriptor.status == studypet::DeckStatus::Invalid) {
      rowValues.emplace_back(tr(STR_STUDY_INVALID));
    } else {
      rowValues.emplace_back(tr(STR_STUDY_UNREADABLE));
    }

    fui::ListItem item;
    item.label = rowLabels.back().c_str();
    item.value = rowValues.back().c_str();
    item.actionValue = static_cast<int16_t>(i);  // bounded by rowLimit above
    rowItems.push_back(item);
  }
}

void DeckListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMargin(fui::Insets{studypet::clampedInset(safe.y + metrics.topPadding + metrics.headerHeight),
                                      studypet::clampedInset(renderer.getScreenWidth() - (safe.x + safe.width)),
                                      studypet::clampedInset(renderer.getScreenHeight() - (safe.y + safe.height)),
                                      studypet::clampedInset(safe.x)});
  screen.spacer(studypet::clampedInset(metrics.verticalSpacing));

  if (scanResult.decks.empty()) {
    const char* status = scanResult.error == studypet::DeckRepositoryError::StorageUnavailable
                             ? tr(STR_STUDY_STORAGE_UNAVAILABLE)
                             : tr(STR_STUDY_NO_DECKS);
    const auto lineHeight = screen.target().lineHeight(screen.theme().bodyText.font);
    const fui::Rect body = screen.body().inset(fui::Insets{0, 8, 0, 8});
    screen.target().text(fui::Rect{body.x, body.y, body.width, lineHeight}, status, screen.theme().bodyText);
    screen.target().text(fui::Rect{body.x, studypet::clampedInset(body.y + lineHeight), body.width, lineHeight},
                         tr(STR_STUDY_DECK_PATH), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  uint16_t rowCount = 0;
  if (!studypet::checkedCast(rowItems.size(), rowCount)) {
    LOG_ERR("Study", "Deck list row count exceeds uint16; clamping");
    rowCount = std::numeric_limits<uint16_t>::max();
  }
  props.items = rowItems.data();
  props.count = rowCount;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

void DeckListActivity::activateIndex(const int index) {
  if (index < 0 || static_cast<std::size_t>(index) >= scanResult.decks.size()) return;
  app.clearTapFlash();

  auto descriptor = scanResult.decks[static_cast<std::size_t>(index)];
  if (descriptor.status == studypet::DeckStatus::Valid) {
    auto loaded = repository.loadDeck(descriptor.filename);
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
