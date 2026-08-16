#include "DeckRepository.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace studypet {
namespace {

constexpr std::size_t READ_BUFFER_SIZE = 256;
constexpr std::size_t DECK_ENTRY_NAME_BUFFER_SIZE = MAX_DECK_COMPONENT_BYTES + 2;
constexpr char MODULE_NAME[] = "STUDY";

bool browserEntryLess(const DeckBrowserEntry& left, const DeckBrowserEntry& right) {
  const bool leftIsFolder = left.type == DeckBrowserEntryType::Folder;
  const bool rightIsFolder = right.type == DeckBrowserEntryType::Folder;
  if (deckEntryLess(leftIsFolder, left.displayName, rightIsFolder, right.displayName)) return true;
  if (deckEntryLess(rightIsFolder, right.displayName, leftIsFolder, left.displayName)) return false;
  return left.location.relativePath() < right.location.relativePath();
}

}  // namespace

bool DeckRepository::buildStoragePath(const std::string_view relativePath, std::string& storagePath) {
  if (!isSafeRelativePath(relativePath, true)) return false;

  storagePath.assign(STUDY_DECK_DIRECTORY);
  if (!relativePath.empty()) {
    storagePath.push_back('/');
    storagePath.append(relativePath.data(), relativePath.size());
  }
  return true;
}

DeckRepositoryError DeckRepository::readDeck(const std::string& storagePath, studycore::Deck& deck,
                                             studycore::DeckParseError& parseError) {
  HalFile file;
  if (!Storage.openFileForRead(MODULE_NAME, storagePath, file) || !file) return DeckRepositoryError::FileOpenFailed;
  if (file.isDirectory()) {
    file.close();
    return DeckRepositoryError::FileOpenFailed;
  }

  studycore::CsvDeckParser parser(deck);
  std::array<char, READ_BUFFER_SIZE> buffer{};
  while (file.available() > 0) {
    const int bytesRead = file.read(buffer.data(), buffer.size());
    if (bytesRead <= 0) {
      file.close();
      deck.cards.clear();
      return DeckRepositoryError::ReadFailed;
    }
    if (!parser.feed(std::string_view(buffer.data(), static_cast<std::size_t>(bytesRead)))) {
      parseError = parser.error();
      file.close();
      return DeckRepositoryError::None;
    }
  }

  file.close();
  if (!parser.finish()) {
    parseError = parser.error();
    return DeckRepositoryError::None;
  }
  parseError = parser.error();
  return DeckRepositoryError::None;
}

DeckDirectoryResult DeckRepository::listDirectory(const std::string_view relativeDirectory) const {
  DeckDirectoryResult result;
  std::string storagePath;
  if (!buildStoragePath(relativeDirectory, storagePath) || !isSafeRelativeDirectory(relativeDirectory)) {
    result.error = DeckRepositoryError::InvalidRelativePath;
    LOG_ERR("Study", "Rejected StudyPet directory path");
    return result;
  }

  LOG_DBG("Study", "Listing StudyPet directory: %s", relativeDirectory.empty() ? "<root>" : storagePath.c_str());
  if (!Storage.ready()) {
    result.error = DeckRepositoryError::StorageUnavailable;
    return result;
  }

  HalFile directory = Storage.open(storagePath.c_str());
  if (!directory) {
    result.error = Storage.exists(storagePath.c_str()) ? DeckRepositoryError::DeckDirectoryOpenFailed
                                                       : DeckRepositoryError::DeckDirectoryMissing;
    return result;
  }
  if (!directory.isDirectory()) {
    directory.close();
    result.error = DeckRepositoryError::DeckDirectoryOpenFailed;
    return result;
  }

  std::vector<DeckBrowserEntry> entries;
  entries.reserve(8);
  directory.rewindDirectory();
  for (HalFile entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
    char filename[DECK_ENTRY_NAME_BUFFER_SIZE]{};
    const std::size_t nameLength = entry.getName(filename, sizeof(filename));
    // getName() returns 0 when extraction fails and fills the buffer to this
    // boundary when the name may have been truncated.
    if (nameLength == 0 || nameLength >= sizeof(filename) - 1) {
      LOG_ERR("Study", "Skipping directory entry with unreadable filename");
      entry.close();
      continue;
    }

    const std::string name(filename);
    if (name.front() == '.') {
      entry.close();
      continue;
    }

    const bool isDirectory = entry.isDirectory();
    if (!isDirectory && !isDeckCandidate(name)) {
      entry.close();
      continue;
    }

    std::string relativePath;
    if (!joinRelativeDeckPath(relativeDirectory, name, relativePath)) {
      LOG_ERR("Study", "Skipping overlong or unsafe StudyPet entry");
      entry.close();
      continue;
    }

    DeckLocation location;
    if (isDirectory) {
      if (!DeckLocation::tryCreate(relativePath, location)) {
        LOG_ERR("Study", "Skipping invalid StudyPet folder path");
        entry.close();
        continue;
      }
      entry.close();
      DeckBrowserEntry folder;
      folder.type = DeckBrowserEntryType::Folder;
      folder.displayName = name;
      folder.location = std::move(location);
      entries.push_back(std::move(folder));
      continue;
    }

    if (!DeckLocation::tryCreateDeck(relativePath, location)) {
      entry.close();
      continue;
    }
    entry.close();

    std::string deckStoragePath;
    if (!buildStoragePath(location.relativePath(), deckStoragePath)) {
      LOG_ERR("Study", "Skipping invalid StudyPet deck location");
      continue;
    }

    DeckBrowserEntry deckEntry;
    deckEntry.type = DeckBrowserEntryType::Deck;
    deckEntry.displayName = deckDisplayName(name);
    deckEntry.location = std::move(location);

    studycore::Deck deck;
    deckEntry.repositoryError = readDeck(deckStoragePath, deck, deckEntry.parseError);
    if (deckEntry.repositoryError != DeckRepositoryError::None) {
      deckEntry.status = DeckStatus::Unreadable;
      LOG_ERR("Study", "Cannot read deck %s", deckEntry.location.relativePath().c_str());
    } else if (deckEntry.parseError.code != studycore::DeckParseErrorCode::None) {
      deckEntry.status = DeckStatus::Invalid;
      LOG_ERR("Study", "Deck %s invalid at row %zu", deckEntry.location.relativePath().c_str(),
              deckEntry.parseError.row);
    } else {
      deckEntry.status = DeckStatus::Valid;
      deckEntry.cardCount = deck.cards.size();
      LOG_DBG("Study", "Discovered deck %s with %zu cards", deckEntry.location.relativePath().c_str(),
              deckEntry.cardCount);
    }
    entries.push_back(std::move(deckEntry));
  }
  directory.close();

  std::sort(entries.begin(), entries.end(), browserEntryLess);
  result.entries = std::move(entries);
  return result;
}

DeckLoadResult DeckRepository::loadDeck(const DeckLocation& location) const {
  DeckLoadResult result;
  if (!isSafeRelativeDeckPath(location.relativePath())) {
    result.error = DeckRepositoryError::SelectedDeckUnavailable;
    return result;
  }

  std::string storagePath;
  if (!buildStoragePath(location.relativePath(), storagePath)) {
    result.error = DeckRepositoryError::SelectedDeckUnavailable;
    return result;
  }

  result.error = readDeck(storagePath, result.deck, result.parseError);
  if (result.error == DeckRepositoryError::None && result.parseError.code == studycore::DeckParseErrorCode::None) {
    LOG_DBG("Study", "Loaded StudyPet deck: %s", location.relativePath().c_str());
  }
  return result;
}

}  // namespace studypet
