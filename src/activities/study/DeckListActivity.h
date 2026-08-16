#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "study/storage/DeckRepository.h"

class DeckListActivity final : public UiListActivity {
 public:
  explicit DeckListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(scanResult.decks.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  void reloadDecks();
  void rebuildRows();

  studypet::DeckRepository repository;
  studypet::DeckListResult scanResult;
  std::vector<std::string> rowLabels;
  std::vector<std::string> rowValues;
  std::vector<freeink::ui::ListItem> rowItems;
};
