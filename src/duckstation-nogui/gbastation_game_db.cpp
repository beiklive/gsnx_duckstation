// SPDX-FileCopyrightText: 2026 GBAStation contributors
// SPDX-License-Identifier: GPL-3.0 OR CC-BY-NC-ND-4.0

#include "gbastation_game_db.h"

#include "common/file_system.h"
#include "common/log.h"
#include "switch_paths.h"

#include <chrono>
#include <ctime>
#include <functional>
#include <limits>
#include <mutex>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

Log_SetChannel(GBAStationGameDB);

namespace GBAStationGameDB {
namespace {

std::mutex s_mutex;
std::string s_active_path;
std::chrono::steady_clock::time_point s_started_at;
bool s_active = false;

std::string NormalizePath(std::string path)
{
  for (char& c : path)
  {
    if (c == '\\')
      c = '/';
  }
  if (path.rfind("sdmc:", 0) == 0)
    path.erase(0, 5);
  while (path.size() > 1 && path[0] == '/' && path[1] == '/')
    path.erase(0, 1);
  return path;
}

std::string FindDatabasePath()
{
  const std::string paths[] = {std::string(GBAStation::SwitchPaths::GameDatabaseFile),
                               std::string(GBAStation::SwitchPaths::LegacyGameDatabaseFile)};
  for (const std::string& path : paths)
  {
    if (FileSystem::FileExists(path.c_str()))
      return path;
  }
  return paths[0];
}

bool ReadDatabase(const std::string& path, rapidjson::Document* document)
{
  const std::optional<std::string> data = FileSystem::ReadFileToString(path.c_str());
  if (!data.has_value())
    return false;
  document->Parse(data->c_str(), data->size());
  return !document->HasParseError() && document->IsArray();
}

bool WriteDatabase(const std::string& path, const rapidjson::Document& document)
{
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  writer.SetIndent(' ', 4);
  document.Accept(writer);
  buffer.Put('\n');
  return FileSystem::WriteStringToFile(path.c_str(), std::string_view(buffer.GetString(), buffer.GetSize()));
}

rapidjson::Value* FindRecord(rapidjson::Document& document, const std::string& rom_path)
{
  const std::string normalized = NormalizePath(rom_path);
  for (rapidjson::Value& value : document.GetArray())
  {
    if (!value.IsObject())
      continue;
    const auto path = value.FindMember("path");
    if (path != value.MemberEnd() && path->value.IsString() && NormalizePath(path->value.GetString()) == normalized)
      return &value;
  }
  return nullptr;
}

std::string CurrentTime()
{
  const std::time_t now = std::time(nullptr);
  std::tm tm_value{};
#ifdef _WIN32
  localtime_s(&tm_value, &now);
#else
  localtime_r(&now, &tm_value);
#endif
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%y-%m-%d %H-%M-%S", &tm_value);
  return buffer;
}

void ReadEntry(const rapidjson::Value& value, Entry* entry)
{
  if (!entry)
    return;
  if (const auto it = value.FindMember("path"); it != value.MemberEnd() && it->value.IsString())
    entry->path = it->value.GetString();
  if (const auto it = value.FindMember("title"); it != value.MemberEnd() && it->value.IsString())
    entry->title = it->value.GetString();
  if (const auto it = value.FindMember("savePath"); it != value.MemberEnd() && it->value.IsString())
    entry->save_path = it->value.GetString();
  if (const auto it = value.FindMember("playCount"); it != value.MemberEnd())
  {
    if (it->value.IsInt64())
      entry->play_count = it->value.GetInt64();
    else if (it->value.IsInt())
      entry->play_count = it->value.GetInt();
    else if (it->value.IsUint())
      entry->play_count = it->value.GetUint();
    else if (it->value.IsUint64() && it->value.GetUint64() <= std::numeric_limits<std::int64_t>::max())
      entry->play_count = static_cast<std::int64_t>(it->value.GetUint64());
  }
  if (const auto it = value.FindMember("playTime"); it != value.MemberEnd())
  {
    if (it->value.IsInt64())
      entry->play_time = it->value.GetInt64();
    else if (it->value.IsInt())
      entry->play_time = it->value.GetInt();
    else if (it->value.IsUint())
      entry->play_time = it->value.GetUint();
    else if (it->value.IsUint64() && it->value.GetUint64() <= std::numeric_limits<std::int64_t>::max())
      entry->play_time = static_cast<std::int64_t>(it->value.GetUint64());
  }
  if (const auto it = value.FindMember("lastPlayed"); it != value.MemberEnd() && it->value.IsString())
    entry->last_played = it->value.GetString();
}

bool UpdateRecord(const std::string& rom_path, const std::function<void(rapidjson::Value&, rapidjson::Document::AllocatorType&)>& update)
{
  const std::string database_path = FindDatabasePath();
  rapidjson::Document document;
  if (!ReadDatabase(database_path, &document))
  {
    Log_WarningPrintf("Unable to read PS1 GameDB '%s'.", database_path.c_str());
    return false;
  }
  rapidjson::Value* record = FindRecord(document, rom_path);
  if (!record)
    return false;
  update(*record, document.GetAllocator());
  if (!WriteDatabase(database_path, document))
  {
    Log_ErrorPrintf("Unable to write PS1 GameDB '%s'.", database_path.c_str());
    return false;
  }
  return true;
}

} // namespace

bool LoadForPath(const std::string& rom_path, Entry* entry)
{
  const std::string database_path = FindDatabasePath();
  rapidjson::Document document;
  if (!ReadDatabase(database_path, &document))
    return false;
  rapidjson::Value* record = FindRecord(document, rom_path);
  if (!record)
    return false;
  ReadEntry(*record, entry);
  return true;
}

void OnGameStarted(const std::string& rom_path)
{
  std::lock_guard lock(s_mutex);
  Entry entry;
  if (!LoadForPath(rom_path, &entry))
  {
    Log_InfoPrintf("No PS1 GameDB record for '%s'.", rom_path.c_str());
    s_active = false;
    return;
  }

  UpdateRecord(rom_path, [](rapidjson::Value& record, rapidjson::Document::AllocatorType& allocator) {
    auto it = record.FindMember("playCount");
    if (it == record.MemberEnd())
      record.AddMember("playCount", rapidjson::Value(1), allocator);
    else if (it->value.IsInt64())
      it->value.SetInt64(it->value.GetInt64() + 1);
    else if (it->value.IsInt())
      it->value.SetInt64(static_cast<std::int64_t>(it->value.GetInt()) + 1);
    else if (it->value.IsUint())
      it->value.SetInt64(static_cast<std::int64_t>(it->value.GetUint()) + 1);
    else if (it->value.IsUint64() && it->value.GetUint64() < std::numeric_limits<std::int64_t>::max())
      it->value.SetInt64(static_cast<std::int64_t>(it->value.GetUint64()) + 1);
  });
  s_active_path = rom_path;
  s_started_at = std::chrono::steady_clock::now();
  s_active = true;
  Log_InfoPrintf("PS1 GameDB started: %s (%s).", entry.title.c_str(), entry.save_path.c_str());
}

void OnGameStopped()
{
  std::lock_guard lock(s_mutex);
  if (!s_active)
    return;
  const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - s_started_at).count();
  const std::string path = s_active_path;
  UpdateRecord(path, [elapsed](rapidjson::Value& record, rapidjson::Document::AllocatorType& allocator) {
    auto it = record.FindMember("playTime");
    std::int64_t old_time = 0;
    if (it != record.MemberEnd())
    {
      if (it->value.IsInt64())
        old_time = it->value.GetInt64();
      else if (it->value.IsInt())
        old_time = it->value.GetInt();
      else if (it->value.IsUint())
        old_time = it->value.GetUint();
      else if (it->value.IsUint64() && it->value.GetUint64() <= std::numeric_limits<std::int64_t>::max())
        old_time = static_cast<std::int64_t>(it->value.GetUint64());
    }
    if (it == record.MemberEnd())
      record.AddMember("playTime", rapidjson::Value(old_time + elapsed), allocator);
    else
      it->value.SetInt64(old_time + elapsed);
    it = record.FindMember("lastPlayed");
    rapidjson::Value timestamp(CurrentTime().c_str(), allocator);
    if (it == record.MemberEnd())
      record.AddMember("lastPlayed", timestamp, allocator);
    else
      it->value = timestamp;
  });
  s_active = false;
  s_active_path.clear();
}

} // namespace GBAStationGameDB
