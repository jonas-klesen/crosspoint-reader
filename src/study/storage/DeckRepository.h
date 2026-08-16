#pragma once

#include <CsvDeckParser.h>

#include <cstddef>
#include <string>
#include <vector>

namespace studypet {

inline constexpr const char* STUDY_DECK_DIRECTORY = "/study/decks";

enum class DeckStatus {
  Valid,
  Invalid,
  Unreadable,
};

enum class DeckRepositoryError {
  None,
  StorageUnavailable,
  DeckDirectoryMissing,
  DeckDirectoryOpenFailed,
  SelectedDeckUnavailable,
  FileOpenFailed,
  ReadFailed,
};

struct DeckDescriptor {
  std::string filename;
  std::string displayName;
  DeckStatus status = DeckStatus::Unreadable;
  std::size_t cardCount = 0;
  studycore::DeckParseError parseError;
  DeckRepositoryError repositoryError = DeckRepositoryError::None;
};

struct DeckListResult {
  std::vector<DeckDescriptor> decks;
  DeckRepositoryError error = DeckRepositoryError::None;

  bool ok() const { return error == DeckRepositoryError::None; }
};

struct DeckLoadResult {
  studycore::Deck deck;
  DeckRepositoryError error = DeckRepositoryError::None;
  studycore::DeckParseError parseError;

  bool ok() const {
    return error == DeckRepositoryError::None && parseError.code == studycore::DeckParseErrorCode::None;
  }
};

class DeckRepository {
 public:
  DeckListResult listDecks();
  DeckLoadResult loadDeck(std::size_t index) const;
  DeckLoadResult loadDeck(const std::string& filename) const;

 private:
  static bool isSafeFilename(const std::string& filename);
  static std::string displayNameFor(const std::string& filename);
  static DeckRepositoryError readDeck(const std::string& path, studycore::Deck& deck,
                                      studycore::DeckParseError& parseError);

  std::vector<DeckDescriptor> descriptors;
};

}  // namespace studypet
