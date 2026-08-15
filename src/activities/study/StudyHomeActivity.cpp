#include "StudyHomeActivity.h"

#include <CsvDeckParser.h>
#include <FreeInkUI.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

StudyHomeActivity::StudyHomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("StudyHome", renderer, mappedInput), UiAppHost(renderer) {}

void StudyHomeActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  app.setScreen(&StudyHomeActivity::screenTrampoline, this);
  requestUpdate();
}

void StudyHomeActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

void StudyHomeActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<StudyHomeActivity*>(user)->buildScreen(screen);
}

void StudyHomeActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.header(tr(STR_STUDY_PET), tr(STR_STUDY_PET_SUBTITLE));
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::MessagePanelProps panel;
  panel.title = tr(STR_STUDY_PET);
  panel.message = tr(STR_STUDY_PET_MESSAGE);
  panel.titleText = screen.theme().titleText;
  panel.messageText = screen.theme().bodyText;
  fui::messagePanel(screen.frame(), screen.body(), panel);
}

void StudyHomeActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderUi();

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
