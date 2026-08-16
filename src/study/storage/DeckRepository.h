#pragma once

#include <CsvDeckParser.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "DeckRepositoryRules.h"

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
  InvalidRelativePath,
  SelectedDeckUnavailable,
  FileOpenFailed,
  ReadFailed,
};

// A source deck's identity is its canonical path relative to STUDY_DECK_DIRECTORY.
// The default value represents the browser root and is not a loadable deck.
class DeckLocation {
 public:
  DeckLocation() = default;

  static bool tryCreate(std::string_view relativePath, DeckLocation& location);
  static bool tryCreateDeck(std::string_view relativePath, DeckLocation& location);

  const std::string& relativePath() const noexcept { return relativePath_; }
  bool empty() const noexcept { return relativePath_.empty(); }

 private:
  std::string relativePath_;
};

struct DeckDescriptor {
  DeckLocation location;
  std::string displayName;
  DeckStatus status = DeckStatus::Unreadable;
  std::size_t cardCount = 0;
  studycore::DeckParseError parseError;
  DeckRepositoryError repositoryError = DeckRepositoryError::None;
};

enum class DeckBrowserEntryType {
  Folder,
  Deck,
};

struct DeckBrowserEntry {
  DeckBrowserEntryType type = DeckBrowserEntryType::Folder;
  std::string displayName;
  DeckLocation location;

  DeckStatus status = DeckStatus::Unreadable;
  std::size_t cardCount = 0;
  studycore::DeckParseError parseError;
  DeckRepositoryError repositoryError = DeckRepositoryError::None;

  bool isFolder() const noexcept { return type == DeckBrowserEntryType::Folder; }

  DeckDescriptor asDeckDescriptor() const {
    DeckDescriptor descriptor;
    descriptor.location = location;
    descriptor.displayName = displayName;
    descriptor.status = status;
    descriptor.cardCount = cardCount;
    descriptor.parseError = parseError;
    descriptor.repositoryError = repositoryError;
    return descriptor;
  }
};

struct DeckDirectoryResult {
  std::vector<DeckBrowserEntry> entries;
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
  DeckDirectoryResult listDirectory(std::string_view relativeDirectory) const;
  DeckLoadResult loadDeck(const DeckLocation& location) const;

 private:
  static bool buildStoragePath(std::string_view relativePath, std::string& storagePath);
  static DeckRepositoryError readDeck(const std::string& storagePath, studycore::Deck& deck,
                                      studycore::DeckParseError& parseError);
};

inline bool DeckLocation::tryCreate(const std::string_view relativePath, DeckLocation& location) {
  if (!isSafeRelativePath(relativePath, true)) return false;
  location.relativePath_ = std::string(relativePath);
  return true;
}

inline bool DeckLocation::tryCreateDeck(const std::string_view relativePath, DeckLocation& location) {
  if (!isSafeRelativeDeckPath(relativePath)) return false;
  location.relativePath_ = std::string(relativePath);
  return true;
}

}  // namespace studypet
