#pragma once

#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "activities/UiListActivity.h"
#include "study/storage/DeckRepository.h"

class DeckListActivity final : public UiListActivity {
 public:
  explicit DeckListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override {
    // rebuildRows() bounds rowItems to INT16_MAX+1 rows (int16 actionValue),
    // so this cast cannot overflow; the guard keeps the conversion explicit.
    if (rowItems.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      return std::numeric_limits<int>::max();
    }
    return static_cast<int>(rowItems.size());
  }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void drawChrome() override;
  void onBackButton() override;

  void reloadDirectory(std::string_view selectedPath = {});
  void rebuildRows();
  int findEntry(std::string_view relativePath) const;

  studypet::DeckRepository repository;
  std::string currentDirectory;
  studypet::DeckDirectoryResult scanResult;
  std::vector<std::string> rowLabels;
  std::vector<std::string> rowValues;
  std::vector<freeink::ui::ListItem> rowItems;
};
