#include "StudyStatsStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <array>
#include <string_view>

#include "StudyStatsJson.h"

namespace studypet {
namespace {

constexpr char MODULE_NAME[] = "StudyStats";
constexpr char STUDY_STATE_DIRECTORY[] = "/.crosspoint/study";
constexpr char CROSSPOINT_DIRECTORY[] = "/.crosspoint";

StudyStatsLoadResult storageFailure(const char* message) {
  LOG_ERR(MODULE_NAME, "%s", message);
  StudyStatsLoadResult result;
  result.status = StudyStatsLoadStatus::StorageError;
  return result;
}

}  // namespace

StudyStatsLoadResult StudyStatsStore::load() const {
  if (!Storage.ready()) return storageFailure("Study stats storage is unavailable");
  if (!Storage.exists(FILE_PATH)) {
    LOG_DBG(MODULE_NAME, "Study stats file is missing");
    StudyStatsLoadResult result;
    result.status = StudyStatsLoadStatus::Missing;
    return result;
  }

  HalFile file;
  if (!Storage.openFileForRead(MODULE_NAME, FILE_PATH, file) || !file || file.isDirectory()) {
    return storageFailure("Study stats file could not be opened");
  }

  const std::size_t fileSize = file.fileSize();
  if (fileSize == 0 || fileSize > StudyStatsJson::MAX_FILE_BYTES) {
    LOG_ERR(MODULE_NAME, "Study stats file has invalid size: %u", static_cast<unsigned int>(fileSize));
    StudyStatsLoadResult result;
    result.status = StudyStatsLoadStatus::Corrupt;
    return result;
  }

  std::array<char, StudyStatsJson::MAX_FILE_BYTES + 1> buffer{};
  std::size_t offset = 0;
  while (offset < fileSize) {
    const int bytesRead = file.read(buffer.data() + offset, fileSize - offset);
    if (bytesRead <= 0 || static_cast<std::size_t>(bytesRead) > fileSize - offset) {
      return storageFailure("Study stats file could not be read");
    }
    offset += static_cast<std::size_t>(bytesRead);
  }

  const StudyStatsJsonDecodeResult decoded = StudyStatsJson::decode(std::string_view(buffer.data(), fileSize));
  if (decoded.ok()) {
    LOG_DBG(MODULE_NAME, "Study stats loaded");
    StudyStatsLoadResult result;
    result.stats = decoded.stats;
    result.status = StudyStatsLoadStatus::Loaded;
    return result;
  }

  if (decoded.error == StudyStatsJsonError::UnsupportedSchema && decoded.schema > 1U) {
    LOG_ERR(MODULE_NAME, "Study stats schema is newer than this firmware: %u",
            static_cast<unsigned int>(decoded.schema));
    StudyStatsLoadResult result;
    result.status = StudyStatsLoadStatus::NewerSchema;
    return result;
  }

  LOG_ERR(MODULE_NAME, "Study stats file is invalid (schema %u)", static_cast<unsigned int>(decoded.schema));
  StudyStatsLoadResult result;
  result.status = StudyStatsLoadStatus::Corrupt;
  return result;
}

bool StudyStatsStore::save(const studycore::StudyStats& stats) const {
  std::array<char, StudyStatsJson::MAX_ENCODED_BYTES> buffer{};
  std::size_t encodedLength = 0;
  if (!StudyStatsJson::encode(stats, buffer.data(), buffer.size(), encodedLength)) {
    LOG_ERR(MODULE_NAME, "Study stats have an invalid invariant");
    return false;
  }

  if (!Storage.ensureDirectoryExists(CROSSPOINT_DIRECTORY) || !Storage.ensureDirectoryExists(STUDY_STATE_DIRECTORY)) {
    LOG_ERR(MODULE_NAME, "Study stats directory could not be created");
    return false;
  }

  bool tempWritten = false;
  {
    HalFile file;
    if (!Storage.openFileForWrite(MODULE_NAME, TEMP_FILE_PATH, file) || !file) {
      LOG_ERR(MODULE_NAME, "Study stats temporary file could not be opened");
    } else {
      const std::size_t bytesWritten = file.write(buffer.data(), encodedLength);
      file.flush();
      tempWritten = bytesWritten == encodedLength;
      if (!tempWritten) {
        LOG_ERR(MODULE_NAME, "Study stats temporary file write was short: %u/%u bytes",
                static_cast<unsigned int>(bytesWritten), static_cast<unsigned int>(encodedLength));
      }
    }
  }

  if (!tempWritten) {
    Storage.remove(TEMP_FILE_PATH);
    return false;
  }

  // SdFat does not replace an existing destination during rename. The brief
  // remove-then-rename window can leave a valid file missing, never half-written.
  if (Storage.exists(FILE_PATH) && !Storage.remove(FILE_PATH)) {
    LOG_ERR(MODULE_NAME, "Study stats old file could not be removed");
    Storage.remove(TEMP_FILE_PATH);
    return false;
  }
  if (!Storage.rename(TEMP_FILE_PATH, FILE_PATH)) {
    LOG_ERR(MODULE_NAME, "Study stats temporary file could not be promoted");
    Storage.remove(TEMP_FILE_PATH);
    return false;
  }

  LOG_DBG(MODULE_NAME, "Study stats saved");
  return true;
}

}  // namespace studypet
