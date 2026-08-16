#include "DeckListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "MappedInputManager.h"
#include "StudyDeckDetailsActivity.h"
#include "StudyFormat.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {

std::string breadcrumbForDirectory(const std::string_view relativeDirectory) {
  std::string breadcrumb = tr(STR_STUDY_DECKS);
  std::size_t componentStart = 0;
  while (componentStart < relativeDirectory.size()) {
    const std::size_t separator = relativeDirectory.find('/', componentStart);
    const std::size_t componentEnd = separator == std::string_view::npos ? relativeDirectory.size() : separator;
    breadcrumb += " / ";
    breadcrumb.append(relativeDirectory.substr(componentStart, componentEnd - componentStart));
    if (separator == std::string_view::npos) break;
    componentStart = separator + 1;
  }
  return breadcrumb;
}

std::string boundedBreadcrumb(const GfxRenderer& renderer, const std::string& breadcrumb, const int maxWidth) {
  if (renderer.getTextWidth(UI_12_FONT_ID, breadcrumb.c_str()) <= maxWidth) return breadcrumb;

  constexpr char ELLIPSIS[] = "\xe2\x80\xa6 / ";
  const int ellipsisWidth = renderer.getTextWidth(UI_12_FONT_ID, ELLIPSIS);
  const std::size_t lastSeparator = breadcrumb.rfind(" / ");
  if (lastSeparator == std::string::npos) {
    return renderer.truncatedText(UI_12_FONT_ID, breadcrumb.c_str(), maxWidth);
  }

  std::size_t suffixStart = lastSeparator + 3;
  while (true) {
    const std::string candidate = std::string(ELLIPSIS) + breadcrumb.substr(suffixStart);
    if (renderer.getTextWidth(UI_12_FONT_ID, candidate.c_str()) <= maxWidth) return candidate;
    const std::size_t searchPosition = suffixStart > 3 ? suffixStart - 4 : std::string::npos;
    const std::size_t previousSeparator =
        searchPosition == std::string::npos ? std::string::npos : breadcrumb.rfind(" / ", searchPosition);
    if (previousSeparator == std::string::npos) break;
    suffixStart = previousSeparator + 3;
  }

  const int available = std::max(1, maxWidth - ellipsisWidth);
  const std::string currentFolder = breadcrumb.substr(lastSeparator + 3);
  return std::string(ELLIPSIS) + renderer.truncatedText(UI_12_FONT_ID, currentFolder.c_str(), available);
}

}  // namespace

DeckListActivity::DeckListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("DeckList", renderer, mappedInput) {}

void DeckListActivity::onEnter() {
  UiListActivity::onEnter();
  reloadDirectory();
}

void DeckListActivity::reloadDirectory(const std::string_view selectedPath) {
  closeRouting();
  scanResult = repository.listDirectory(currentDirectory);
  nav.reset();
  rebuildRows();
  if (!selectedPath.empty() && !rowItems.empty()) {
    nav.selected = findEntry(selectedPath);
    nav.follow(listCount());
  }
}

void DeckListActivity::rebuildRows() {
  rowLabels.clear();
  rowValues.clear();
  rowItems.clear();

  // Rows carry an int16 action value; once the entry count exceeds that range
  // every further row would wrap, so stop listing there rather than showing
  // corrupted selections.
  const std::size_t rowLimit =
      std::min(scanResult.entries.size(), static_cast<std::size_t>(std::numeric_limits<int16_t>::max()) + 1);
  if (scanResult.entries.size() > rowLimit) {
    LOG_ERR("Study", "StudyPet directory has %zu entries; showing the first %zu", scanResult.entries.size(), rowLimit);
  }
  rowLabels.reserve(rowLimit);
  rowValues.reserve(rowLimit);
  rowItems.reserve(rowLimit);

  for (std::size_t i = 0; i < rowLimit; ++i) {
    const auto& entry = scanResult.entries[i];
    rowLabels.push_back(entry.isFolder() ? entry.displayName + "/" : entry.displayName);

    if (entry.isFolder()) {
      rowValues.emplace_back();
    } else if (entry.status == studypet::DeckStatus::Valid) {
      char value[32]{};
      studypet::safeFormat(value, sizeof(value), tr(STR_STUDY_INVALID), tr(STR_STUDY_CARD_COUNT_FORMAT),
                           entry.cardCount);
      rowValues.emplace_back(value);
    } else if (entry.status == studypet::DeckStatus::Invalid) {
      rowValues.emplace_back(tr(STR_STUDY_INVALID));
    } else {
      rowValues.emplace_back(tr(STR_STUDY_UNREADABLE));
    }

    fui::ListItem item;
    item.label = rowLabels.back().c_str();
    if (!rowValues.back().empty()) item.value = rowValues.back().c_str();
    item.icon = listIconFor(entry.isFolder() ? UIIcon::Folder : UIIcon::Book);
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

  if (scanResult.entries.empty()) {
    const char* status = tr(STR_STUDY_NO_DECKS);
    if (scanResult.error == studypet::DeckRepositoryError::StorageUnavailable) {
      status = tr(STR_STUDY_STORAGE_UNAVAILABLE);
    } else if (!currentDirectory.empty()) {
      status = scanResult.error == studypet::DeckRepositoryError::DeckDirectoryOpenFailed ||
                       scanResult.error == studypet::DeckRepositoryError::InvalidRelativePath
                   ? tr(STR_STUDY_ERROR_DIRECTORY)
                   : tr(STR_STUDY_NO_DECKS_OR_FOLDERS);
    }

    const auto lineHeight = screen.target().lineHeight(screen.theme().bodyText.font);
    const fui::Rect body = screen.body().inset(fui::Insets{0, 8, 0, 8});
    screen.target().text(fui::Rect{body.x, body.y, body.width, lineHeight}, status, screen.theme().bodyText);
    const std::string path = currentDirectory.empty() ? std::string(tr(STR_STUDY_DECK_PATH)) : currentDirectory;
    const std::string pathDisplay = renderer.truncatedText(SMALL_FONT_ID, path.c_str(), body.width);
    screen.target().text(fui::Rect{body.x, studypet::clampedInset(body.y + lineHeight), body.width, lineHeight},
                         pathDisplay.c_str(), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  uint16_t rowCount = 0;
  if (!studypet::checkedCast(rowItems.size(), rowCount)) {
    LOG_ERR("Study", "StudyPet row count exceeds uint16; clamping");
    rowCount = std::numeric_limits<uint16_t>::max();
  }
  props.items = rowItems.data();
  props.count = rowCount;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

void DeckListActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const std::string breadcrumb = breadcrumbForDirectory(currentDirectory);
  const int maxWidth = std::max(1, renderer.getScreenWidth() / 2);
  const std::string title = boundedBreadcrumb(renderer, breadcrumb, maxWidth);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, title.c_str());
}

int DeckListActivity::findEntry(const std::string_view relativePath) const {
  for (std::size_t i = 0; i < rowItems.size(); ++i) {
    if (scanResult.entries[i].location.relativePath() == relativePath) return static_cast<int>(i);
  }
  return 0;
}

void DeckListActivity::activateIndex(const int index) {
  if (index < 0 || static_cast<std::size_t>(index) >= rowItems.size()) return;
  app.clearTapFlash();

  const auto& entry = scanResult.entries[static_cast<std::size_t>(index)];
  if (entry.isFolder()) {
    const std::string nextDirectory = entry.location.relativePath();
    {
      RenderLock lock(*this);
      currentDirectory = nextDirectory;
      reloadDirectory();
    }
    requestUpdate();
    return;
  }

  auto descriptor = entry.asDeckDescriptor();
  if (descriptor.status == studypet::DeckStatus::Valid) {
    auto loaded = repository.loadDeck(descriptor.location);
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

  const std::string selectedPath = descriptor.location.relativePath();
  auto details = makeUniqueNoThrow<StudyDeckDetailsActivity>(renderer, mappedInput, std::move(descriptor));
  if (!details) {
    LOG_ERR("Study", "OOM: deck details activity");
    return;
  }
  startActivityForResult(std::move(details), [this, selectedPath](const ActivityResult&) {
    RenderLock lock(*this);
    reloadDirectory(selectedPath);
  });
}

void DeckListActivity::onBackButton() {
  if (currentDirectory.empty()) {
    finish();
    return;
  }

  const std::string previousDirectory = currentDirectory;
  const std::size_t separator = previousDirectory.find_last_of('/');
  const std::string parentDirectory =
      separator == std::string::npos ? std::string{} : previousDirectory.substr(0, separator);
  {
    RenderLock lock(*this);
    currentDirectory = parentDirectory;
    reloadDirectory(previousDirectory);
  }
  requestUpdate();
}
