// SPDX-FileCopyrightText: 2026 GBAStation contributors
// SPDX-License-Identifier: GPL-3.0 OR CC-BY-NC-ND-4.0

#include "gbastation_config.h"

#include "common/log.h"
#include "common/settings_interface.h"
#include "switch_paths.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <initializer_list>

Log_SetChannel(GBAStationConfig);

namespace GBAStationConfig {
namespace {

static std::string Trim(std::string_view value)
{
  size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
    begin++;
  size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
    end--;
  return std::string(value.substr(begin, end - begin));
}

static std::string Unescape(std::string_view value)
{
  std::string result;
  result.reserve(value.size());
  bool escaped = false;
  for (const char c : value)
  {
    if (escaped)
    {
      result.push_back(c);
      escaped = false;
    }
    else if (c == '\\')
    {
      escaped = true;
    }
    else
    {
      result.push_back(c);
    }
  }
  if (escaped)
    result.push_back('\\');
  return result;
}

static std::string DecodeValue(std::string value)
{
  const size_t separator = value.find('|');
  if (separator != std::string::npos)
    value.erase(0, separator + 1);
  return Unescape(value);
}

static const std::string* Find(const Values& values, std::initializer_list<std::string> keys)
{
  for (const std::string& key : keys)
  {
    const auto it = values.find(key);
    if (it != values.end())
      return &it->second;
  }
  return nullptr;
}

static bool ParseBool(const std::string& value, bool default_value = false)
{
  std::string lower(value);
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (lower == "1" || lower == "true" || lower == "enabled" || lower == "yes" || lower == "on")
    return true;
  if (lower == "0" || lower == "false" || lower == "disabled" || lower == "no" || lower == "off")
    return false;
  return default_value;
}

static void SetString(SettingsInterface& settings, const Values& values, std::initializer_list<std::string> keys,
                      const char* section, const char* key)
{
  if (const std::string* value = Find(values, keys))
    settings.SetStringValue(section, key, value->c_str());
}

static void SetBool(SettingsInterface& settings, const Values& values, std::initializer_list<std::string> keys,
                    const char* section, const char* key)
{
  if (const std::string* value = Find(values, keys))
    settings.SetBoolValue(section, key, ParseBool(*value));
}

static void SetInt(SettingsInterface& settings, const Values& values, std::initializer_list<std::string> keys,
                   const char* section, const char* key)
{
  if (const std::string* value = Find(values, keys))
  {
    char* end = nullptr;
    const long parsed = std::strtol(value->c_str(), &end, 10);
    if (end != value->c_str() && *end == '\0')
      settings.SetIntValue(section, key, static_cast<s32>(parsed));
    else
      Log_WarningPrintf("Invalid integer in config.cfg for %s.%s: %s", section, key, value->c_str());
  }
}

static void SetUInt(SettingsInterface& settings, const Values& values, std::initializer_list<std::string> keys,
                    const char* section, const char* key)
{
  if (const std::string* value = Find(values, keys))
  {
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value->c_str(), &end, 10);
    if (end != value->c_str() && *end == '\0')
      settings.SetUIntValue(section, key, static_cast<u32>(parsed));
    else
      Log_WarningPrintf("Invalid unsigned integer in config.cfg for %s.%s: %s", section, key, value->c_str());
  }
}

static void SetFloat(SettingsInterface& settings, const Values& values, std::initializer_list<std::string> keys,
                     const char* section, const char* key)
{
  if (const std::string* value = Find(values, keys))
  {
    char* end = nullptr;
    const float parsed = std::strtof(value->c_str(), &end);
    if (end != value->c_str() && *end == '\0')
      settings.SetFloatValue(section, key, parsed);
    else
      Log_WarningPrintf("Invalid float in config.cfg for %s.%s: %s", section, key, value->c_str());
  }
}

} // namespace

bool Load(Values* values, std::string* loaded_path)
{
  if (!values)
    return false;
  values->clear();

  const std::string paths[] = {std::string(GBAStation::SwitchPaths::ConfigFile),
                               std::string(GBAStation::SwitchPaths::LegacyConfigFile)};
  std::ifstream input;
  std::string selected_path;
  for (const std::string& path : paths)
  {
    input.open(path);
    if (input.is_open())
    {
      selected_path = path;
      break;
    }
    input.clear();
  }
  if (!input.is_open())
  {
    Log_InfoPrintf("GBAStation config.cfg was not found.");
    return false;
  }

  std::string line;
  while (std::getline(input, line))
  {
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#')
      continue;
    const size_t separator = trimmed.find('=');
    if (separator == std::string::npos)
      continue;
    const std::string key = Trim(std::string_view(trimmed).substr(0, separator));
    if (key.empty())
      continue;
    (*values)[key] = DecodeValue(Trim(std::string_view(trimmed).substr(separator + 1)));
  }

  if (loaded_path)
    *loaded_path = selected_path;
  Log_InfoPrintf("Loaded %zu GBAStation config keys from '%s'.", values->size(), selected_path.c_str());
  return true;
}

void ApplyCoreSettings(const Values& values, SettingsInterface& settings)
{
  // 系统 / System
  SetString(settings, values, {"core.ps1.region", "ps1.region"}, "Console", "Region");
  SetBool(settings, values, {"core.ps1.enable8MBRAM", "ps1.enable8MBRAM"}, "Console", "Enable8MBRAM");
  SetBool(settings, values, {"core.ps1.enableCheats", "ps1.enableCheats"}, "Console", "EnableCheats");
  SetBool(settings, values, {"core.ps1.disableAllEnhancements", "ps1.disableAllEnhancements"}, "Main",
          "DisableAllEnhancements");

  // 性能 / Emulation
  SetFloat(settings, values, {"core.ps1.emulationSpeed", "ps1.emulationSpeed"}, "Main", "EmulationSpeed");
  SetFloat(settings, values, {"core.ps1.fastForwardSpeed", "ps1.fastForwardSpeed"}, "Main", "FastForwardSpeed");
  SetFloat(settings, values, {"core.ps1.turboSpeed", "ps1.turboSpeed"}, "Main", "TurboSpeed");
  SetBool(settings, values, {"core.ps1.syncToHostRefreshRate", "ps1.syncToHostRefreshRate"}, "Main",
          "SyncToHostRefreshRate");
  SetUInt(settings, values, {"core.ps1.runaheadFrameCount", "ps1.runaheadFrameCount"}, "Main", "RunaheadFrameCount");
  SetBool(settings, values, {"core.ps1.rewindEnable", "ps1.rewindEnable"}, "Main", "RewindEnable");
  SetFloat(settings, values, {"core.ps1.rewindFrequency", "ps1.rewindFrequency"}, "Main", "RewindFrequency");
  SetInt(settings, values, {"core.ps1.rewindSaveSlots", "ps1.rewindSaveSlots"}, "Main", "RewindSaveSlots");

  // 音频 / Audio
  SetInt(settings, values, {"core.ps1.outputVolume", "ps1.outputVolume"}, "Audio", "OutputVolume");
  SetInt(settings, values, {"core.ps1.fastForwardVolume", "ps1.fastForwardVolume"}, "Audio", "FastForwardVolume");
  SetBool(settings, values, {"core.ps1.outputMuted", "ps1.outputMuted"}, "Audio", "OutputMuted");
  SetString(settings, values, {"core.ps1.backend", "ps1.backend"}, "Audio", "Backend");
  SetString(settings, values, {"core.ps1.stretchMode", "ps1.stretchMode"}, "Audio", "StretchMode");
  SetUInt(settings, values, {"core.ps1.outputLatencyMS", "ps1.outputLatencyMS"}, "Audio", "OutputLatencyMS");
  SetUInt(settings, values, {"core.ps1.bufferMS", "ps1.bufferMS"}, "Audio", "BufferMS");

  // 存档 / Save states
  SetBool(settings, values, {"core.ps1.saveStateOnExit", "ps1.saveStateOnExit"}, "Main", "SaveStateOnExit");
  SetBool(settings, values, {"core.ps1.createSaveStateBackups", "ps1.createSaveStateBackups"}, "Main",
          "CreateSaveStateBackups");
  SetBool(settings, values, {"core.ps1.loadDevicesFromSaveStates", "ps1.loadDevicesFromSaveStates"}, "Main",
          "LoadDevicesFromSaveStates");

  // 记忆卡 / Memory cards
  SetString(settings, values, {"core.ps1.memoryCardDirectory", "ps1.memoryCardDirectory"}, "MemoryCards", "Directory");
  SetBool(settings, values, {"core.ps1.usePlaylistTitle", "ps1.usePlaylistTitle"}, "MemoryCards", "UsePlaylistTitle");
  SetString(settings, values, {"core.ps1.card1Type", "ps1.card1Type"}, "MemoryCards", "Card1Type");
  SetString(settings, values, {"core.ps1.card2Type", "ps1.card2Type"}, "MemoryCards", "Card2Type");
  SetString(settings, values, {"core.ps1.card1Path", "ps1.card1Path"}, "MemoryCards", "Card1Path");
  SetString(settings, values, {"core.ps1.card2Path", "ps1.card2Path"}, "MemoryCards", "Card2Path");

  // BIOS
  SetString(settings, values, {"core.ps1.biosPathNTSCJ", "ps1.biosPathNTSCJ"}, "BIOS", "PathNTSCJ");
  SetString(settings, values, {"core.ps1.biosPathNTSCU", "ps1.biosPathNTSCU"}, "BIOS", "PathNTSCU");
  SetString(settings, values, {"core.ps1.biosPathPAL", "ps1.biosPathPAL"}, "BIOS", "PathPAL");
  SetBool(settings, values, {"core.ps1.ttyLogging", "ps1.ttyLogging"}, "BIOS", "TTYLogging");
  SetBool(settings, values, {"core.ps1.fastBoot", "ps1.fastBoot"}, "BIOS", "PatchFastBoot");

  // 纹理替换 / Texture replacements
  SetBool(settings, values, {"core.ps1.enableVRAMWriteReplacements", "ps1.enableVRAMWriteReplacements"},
          "TextureReplacements", "EnableVRAMWriteReplacements");
  SetBool(settings, values, {"core.ps1.preloadTextures", "ps1.preloadTextures"}, "TextureReplacements",
          "PreloadTextures");
  SetBool(settings, values, {"core.ps1.dumpVRAMWrites", "ps1.dumpVRAMWrites"}, "TextureReplacements", "DumpVRAMWrites");
  SetBool(settings, values, {"core.ps1.dumpVRAMWriteForceAlphaChannel", "ps1.dumpVRAMWriteForceAlphaChannel"},
          "TextureReplacements", "DumpVRAMWriteForceAlphaChannel");

  // 日志 / Logging
  SetString(settings, values, {"core.ps1.logLevel", "ps1.logLevel"}, "Logging", "LogLevel");
  SetBool(settings, values, {"core.ps1.logToConsole", "ps1.logToConsole"}, "Logging", "LogToConsole");
  SetBool(settings, values, {"core.ps1.logToDebug", "ps1.logToDebug"}, "Logging", "LogToDebug");
  SetBool(settings, values, {"core.ps1.logToFile", "ps1.logToFile"}, "Logging", "LogToFile");
}

} // namespace GBAStationConfig
