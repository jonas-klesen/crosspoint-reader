#include "StudyDeckDetailsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <memory>
#include <utility>

#include "MappedInputManager.h"
#include "ReviewActivity.h"
#include "StudyFormat.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {

const char* parseErrorLabel(const studycore::DeckParseErrorCode code) {
  switch (code) {
    case studycore::DeckParseErrorCode::InvalidHeader:
      return tr(STR_STUDY_ERROR_HEADER);
    case studycore::DeckParseErrorCode::DuplicateCardId:
      return tr(STR_STUDY_ERROR_DUPLICATE);
    case studycore::DeckParseErrorCode::MalformedCsv:
    case studycore::DeckParseErrorCode::UnexpectedEndOfInput:
      return tr(STR_STUDY_ERROR_MALFORMED);
    default:
      return tr(STR_STUDY_ERROR_INVALID);
  }
}

}  // namespace

StudyDeckDetailsActivity::StudyDeckDetailsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                   studypet::DeckDescriptor descriptor)
    : Activity("StudyDeckDetails", renderer, mappedInput), UiAppHost(renderer), descriptor(std::move(descriptor)) {}

void StudyDeckDetailsActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  buildMessage();
  app.setScreen(&StudyDeckDetailsActivity::screenTrampoline, this);
  requestUpdate();
}

void StudyDeckDetailsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) startReview();
}

void StudyDeckDetailsActivity::buildMessage() {
  if (descriptor.status == studypet::DeckStatus::Valid) {
    char count[48]{};
    studypet::safeFormat(count, sizeof(count), tr(STR_STUDY_INVALID), tr(STR_STUDY_CARD_COUNT_FORMAT),
                         descriptor.cardCount);
    message = count;
    message += "\n";
    message += descriptor.cardCount == 0 ? tr(STR_STUDY_NO_CARDS) : tr(STR_STUDY_START_REVIEW);
    return;
  }

  if (descriptor.status == studypet::DeckStatus::Unreadable) {
    message = tr(STR_STUDY_ERROR_STORAGE);
    return;
  }

  message = parseErrorLabel(descriptor.parseError.code);
  if (descriptor.parseError.row != 0) {
    char row[32]{};
    studypet::safeFormat(row, sizeof(row), tr(STR_STUDY_ERROR_INVALID), tr(STR_STUDY_ERROR_ROW_FORMAT),
                         descriptor.parseError.row);
    message += "\n";
    message += row;
  }
  if (!descriptor.parseError.duplicateId.empty()) {
    message += "\n";
    message += descriptor.parseError.duplicateId;
  }
}

void StudyDeckDetailsActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<StudyDeckDetailsActivity*>(user)->buildScreen(screen);
}

void StudyDeckDetailsActivity::startReview() {
  if (descriptor.status != studypet::DeckStatus::Valid || descriptor.cardCount == 0) return;

  auto loaded = repository.loadDeck(descriptor.filename);
  if (!loaded.ok()) {
    descriptor.status = loaded.parseError.code == studycore::DeckParseErrorCode::None ? studypet::DeckStatus::Unreadable
                                                                                      : studypet::DeckStatus::Invalid;
    descriptor.repositoryError = loaded.error;
    descriptor.parseError = loaded.parseError;
    buildMessage();
    requestUpdate();
    return;
  }
  if (loaded.deck.cards.empty()) {
    descriptor.cardCount = 0;
    buildMessage();
    requestUpdate();
    return;
  }

  auto review =
      makeUniqueNoThrow<ReviewActivity>(renderer, mappedInput, std::move(loaded.deck), descriptor.displayName);
  if (!review) {
    LOG_ERR("Study", "OOM: review activity");
    return;
  }
  app.clearTapFlash();
  startActivityForResult(std::move(review), nullptr);
}

void StudyDeckDetailsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(
      fui::Insets{studypet::clampedInset(metrics.topPadding), 0, studypet::clampedInset(metrics.buttonHintsHeight), 0});
  screen.header(tr(STR_STUDY_DECK_DETAILS), descriptor.filename.c_str());
  screen.spacer(studypet::clampedInset(metrics.verticalSpacing));

  const fui::Rect body = screen.body();
  const auto titleHeight = screen.target().lineHeight(screen.theme().titleText.font);
  const char* status = tr(STR_STUDY_INVALID);
  if (descriptor.status == studypet::DeckStatus::Valid) status = tr(STR_STUDY_READY);
  if (descriptor.status == studypet::DeckStatus::Unreadable) status = tr(STR_STUDY_UNREADABLE);
  screen.target().text(fui::Rect{body.x, body.y, body.width, titleHeight}, status, screen.theme().titleText);

  fui::TextAreaProps textProps;
  textProps.text = message.c_str();
  textProps.style = screen.theme().bodyText;
  textProps.showCaret = false;
  screen.spacer(studypet::clampedInset(titleHeight + metrics.verticalSpacing));
  screen.textArea(textProps);
}

void StudyDeckDetailsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();
  const auto labels = mappedInput.mapLabels(
      tr(STR_BACK), descriptor.status == studypet::DeckStatus::Valid && descriptor.cardCount > 0 ? tr(STR_SELECT) : "",
      "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
