#pragma once

#include <array>

#include "activities/UiListActivity.h"

class StudyHomeActivity final : public UiListActivity {
 public:
  explicit StudyHomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override { return 2; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::array<freeink::ui::ListItem, 2> rowItems{};
};
