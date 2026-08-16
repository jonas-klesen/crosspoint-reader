#include "StudyDeckDetailsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
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
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) finish();
}

void StudyDeckDetailsActivity::buildMessage() {
  if (descriptor.status == studypet::DeckStatus::Valid) {
    char count[48]{};
    std::snprintf(count, sizeof(count), tr(STR_STUDY_CARD_COUNT_FORMAT), descriptor.cardCount);
    message = count;
    message += "\n";
    message += tr(STR_STUDY_REVIEW_NOT_IMPLEMENTED);
    return;
  }

  if (descriptor.status == studypet::DeckStatus::Unreadable) {
    message = tr(STR_STUDY_ERROR_STORAGE);
    return;
  }

  message = parseErrorLabel(descriptor.parseError.code);
  if (descriptor.parseError.row != 0) {
    char row[32]{};
    std::snprintf(row, sizeof(row), tr(STR_STUDY_ERROR_ROW_FORMAT), descriptor.parseError.row);
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

void StudyDeckDetailsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding), 0, static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.header(tr(STR_STUDY_DECK_DETAILS), descriptor.filename.c_str());
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

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
  screen.spacer(static_cast<int16_t>(titleHeight + metrics.verticalSpacing));
  screen.textArea(textProps);
}

void StudyDeckDetailsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
