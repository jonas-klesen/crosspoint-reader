#pragma once

#include <string>

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "study/storage/DeckRepository.h"

class StudyDeckDetailsActivity final : public Activity, private UiAppHost {
 public:
  StudyDeckDetailsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, studypet::DeckDescriptor descriptor);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static void screenTrampoline(UiScreen& screen, void* user);
  void buildScreen(UiScreen& screen);
  void buildMessage();
  void startReview();

  studypet::DeckRepository repository;
  studypet::DeckDescriptor descriptor;
  std::string message;
};
