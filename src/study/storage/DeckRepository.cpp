#include "DeckRepository.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

#include "DeckRepositoryRules.h"

namespace studypet {
namespace {

constexpr size_t READ_BUFFER_SIZE = 256;
constexpr char MODULE_NAME[] = "STUDY";

bool naturalFilenameLess(const std::string& left, const std::string& right) {
  if (FsHelpers::naturalLess(left, right)) return true;
  if (FsHelpers::naturalLess(right, left)) return false;
  return left < right;
}

std::string deckPath(const std::string& filename) { return std::string(STUDY_DECK_DIRECTORY) + "/" + filename; }

}  // namespace

bool DeckRepository::isSafeFilename(const std::string& filename) { return isSafeDeckFilename(filename); }

std::string DeckRepository::displayNameFor(const std::string& filename) { return deckDisplayName(filename); }

DeckRepositoryError DeckRepository::readDeck(const std::string& path, studycore::Deck& deck,
                                             studycore::DeckParseError& parseError) {
  HalFile file;
  if (!Storage.openFileForRead(MODULE_NAME, path, file) || !file) return DeckRepositoryError::FileOpenFailed;
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

DeckListResult DeckRepository::listDecks() {
  DeckListResult result;
  std::vector<DeckDescriptor> descriptors;

  if (!Storage.ready()) {
    result.error = DeckRepositoryError::StorageUnavailable;
    return result;
  }

  HalFile directory = Storage.open(STUDY_DECK_DIRECTORY);
  if (!directory) {
    result.error = Storage.exists(STUDY_DECK_DIRECTORY) ? DeckRepositoryError::DeckDirectoryOpenFailed
                                                        : DeckRepositoryError::DeckDirectoryMissing;
    return result;
  }
  if (!directory.isDirectory()) {
    directory.close();
    result.error = DeckRepositoryError::DeckDirectoryOpenFailed;
    return result;
  }

  std::vector<std::string> filenames;
  filenames.reserve(8);
  directory.rewindDirectory();
  for (HalFile entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
    char filename[256]{};
    const size_t nameLength = entry.getName(filename, sizeof(filename));
    // getName() returns 0 when the name cannot be extracted and, in the worst
    // case, fills the buffer exactly (capacity minus the terminator) when the
    // name is truncated or unverifiable. Treat both as absent rather than
    // parsing a partial or guessed name.
    if (nameLength == 0 || nameLength >= sizeof(filename) - 1) {
      LOG_ERR("Study", "Skipping directory entry with unreadable filename");
      entry.close();
      continue;
    }
    const std::string name(filename);
    const bool candidate = !entry.isDirectory() && isDeckCandidate(name);
    if (candidate) filenames.push_back(name);
    entry.close();
  }
  directory.close();

  std::sort(filenames.begin(), filenames.end(), naturalFilenameLess);
  descriptors.reserve(filenames.size());
  for (const std::string& filename : filenames) {
    DeckDescriptor descriptor;
    descriptor.filename = filename;
    descriptor.displayName = displayNameFor(filename);

    studycore::Deck deck;
    descriptor.repositoryError = readDeck(deckPath(filename), deck, descriptor.parseError);
    if (descriptor.repositoryError != DeckRepositoryError::None) {
      descriptor.status = DeckStatus::Unreadable;
      LOG_ERR("Study", "Cannot read deck %s", filename.c_str());
    } else if (descriptor.parseError.code != studycore::DeckParseErrorCode::None) {
      descriptor.status = DeckStatus::Invalid;
      LOG_ERR("Study", "Deck %s invalid at row %zu", filename.c_str(), descriptor.parseError.row);
    } else {
      descriptor.status = DeckStatus::Valid;
      descriptor.cardCount = deck.cards.size();
      LOG_DBG("Study", "Discovered deck %s with %zu cards", filename.c_str(), descriptor.cardCount);
    }
    descriptors.push_back(std::move(descriptor));
  }

  result.decks = std::move(descriptors);
  return result;
}

DeckLoadResult DeckRepository::loadDeck(const std::string& filename) const {
  DeckLoadResult result;
  if (!isSafeFilename(filename) || !hasCsvExtension(filename)) {
    result.error = DeckRepositoryError::SelectedDeckUnavailable;
    return result;
  }

  result.error = readDeck(deckPath(filename), result.deck, result.parseError);
  return result;
}

}  // namespace studypet
