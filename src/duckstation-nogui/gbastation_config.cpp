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

static std::string SwitchBinding(std::string value)
{
  if (value.empty() || value == "none")
    return {};

  static constexpr struct {
    const char* name;
    const char* binding;
  } kBindings[] = {
    {"PAD_A", "P0/A"},       {"PAD_B", "P0/B"},         {"PAD_X", "P0/X"},
    {"PAD_Y", "P0/Y"},       {"PAD_LB", "P0/L"},        {"PAD_RB", "P0/R"},
    {"PAD_LT", "P0/ZL"},     {"PAD_RT", "P0/ZR"},       {"PAD_LSB", "P0/LStick"},
    {"PAD_RSB", "P0/RStick"}, {"PAD_BACK", "P0/Minus"}, {"PAD_START", "P0/Plus"},
    {"PAD_LEFT", "P0/DPadLeft"}, {"PAD_UP", "P0/DPadUp"},
    {"PAD_RIGHT", "P0/DPadRight"}, {"PAD_DOWN", "P0/DPadDown"},
  };

  for (const auto& entry : kBindings)
  {
    if (value == entry.name)
      return entry.binding;
  }
  return {};
}

static std::string SwitchChord(const std::string& value)
{
  std::string result;
  size_t start = 0;
  while (start <= value.size())
  {
    const size_t end = value.find('+', start);
    const std::string part = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
    const std::string converted = SwitchBinding(part);
    if (!converted.empty())
    {
      if (!result.empty())
        result.push_back('&');
      result += converted;
    }
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return result;
}

static void SetBinding(SettingsInterface& settings, const Values& values, const char* source_key,
                       const char* section, const char* key)
{
  const auto it = values.find(source_key);
  if (it == values.end())
    return;
  const std::string converted = SwitchChord(it->second);
  if (converted.empty())
    settings.DeleteValue(section, key);
  else
    settings.SetStringValue(section, key, converted.c_str());
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
  SetString(settings, values, {"core.ps1.region", "ps1.region"}, "Console", "Region");
  SetBool(settings, values, {"core.ps1.enable8MBRAM", "ps1.enable8MBRAM"}, "Console", "Enable8MBRAM");
  SetBool(settings, values, {"core.ps1.enableCheats", "ps1.enableCheats"}, "Console", "EnableCheats");

  SetFloat(settings, values, {"core.ps1.emulationSpeed", "ps1.emulationSpeed"}, "Main", "EmulationSpeed");
  SetBool(settings, values, {"core.ps1.syncToHostRefreshRate", "ps1.syncToHostRefreshRate"}, "Main", "SyncToHostRefreshRate");
  SetUInt(settings, values, {"core.ps1.runaheadFrameCount", "ps1.runaheadFrameCount"}, "Main", "RunaheadFrameCount");
  SetBool(settings, values, {"core.ps1.saveStateOnExit", "ps1.saveStateOnExit"}, "Main", "SaveStateOnExit");
  SetBool(settings, values, {"core.ps1.createSaveStateBackups", "ps1.createSaveStateBackups"}, "Main", "CreateSaveStateBackups");

  SetString(settings, values, {"core.ps1.executionMode", "ps1.executionMode"}, "CPU", "ExecutionMode");
  SetBool(settings, values, {"core.ps1.overclockEnable", "ps1.overclockEnable"}, "CPU", "OverclockEnable");
  SetString(settings, values, {"core.ps1.fastmemMode", "ps1.fastmemMode"}, "CPU", "FastmemMode");

  SetString(settings, values, {"core.ps1.renderer", "ps1.renderer"}, "GPU", "Renderer");
  SetInt(settings, values, {"core.ps1.resolutionScale", "ps1.resolutionScale"}, "GPU", "ResolutionScale");
  SetInt(settings, values, {"core.ps1.multisamples", "ps1.multisamples"}, "GPU", "Multisamples");
  SetBool(settings, values, {"core.ps1.trueColor", "ps1.trueColor"}, "GPU", "TrueColor");
  SetBool(settings, values, {"core.ps1.widescreenHack", "ps1.widescreenHack"}, "GPU", "WidescreenHack");
  SetBool(settings, values, {"core.ps1.pgxpEnable", "ps1.pgxpEnable"}, "GPU", "PGXPEnable");
  SetBool(settings, values, {"core.ps1.pgxpTextureCorrection", "ps1.pgxpTextureCorrection"}, "GPU", "PGXPTextureCorrection");

  SetString(settings, values, {"core.ps1.deinterlacingMode", "ps1.deinterlacingMode"}, "Display", "DeinterlacingMode");
  if (const std::string* crop = Find(values, {"core.ps1.cropMode", "ps1.cropMode"}))
    settings.SetStringValue("Display", "CropMode", (*crop == "Auto") ? "None" : crop->c_str());
  SetString(settings, values, {"core.ps1.aspectRatio", "ps1.aspectRatio"}, "Display", "AspectRatio");
  SetBool(settings, values, {"core.ps1.vsync", "ps1.vsync"}, "Display", "VSync");
  SetBool(settings, values, {"core.ps1.showFPS", "ps1.showFPS"}, "Display", "ShowFPS");

  SetInt(settings, values, {"core.ps1.readaheadSectors", "ps1.readaheadSectors"}, "CDROM", "ReadaheadSectors");
  SetInt(settings, values, {"core.ps1.readSpeedup", "ps1.readSpeedup"}, "CDROM", "ReadSpeedup");

  SetUInt(settings, values, {"core.ps1.outputLatencyMS", "ps1.outputLatencyMS"}, "Audio", "OutputLatencyMS");
  SetUInt(settings, values, {"core.ps1.bufferMS", "ps1.bufferMS"}, "Audio", "BufferMS");
  SetBool(settings, values, {"core.ps1.outputMuted", "ps1.outputMuted"}, "Audio", "OutputMuted");

  SetString(settings, values, {"core.ps1.logLevel", "ps1.logLevel"}, "Logging", "LogLevel");
  SetBool(settings, values, {"core.ps1.ttyLogging", "ps1.ttyLogging"}, "BIOS", "TTYLogging");
  SetBool(settings, values, {"core.ps1.fastBoot", "ps1.fastBoot"}, "BIOS", "PatchFastBoot");
}

void ApplyInputBindings(const Values& values, SettingsInterface& settings)
{
  // GBAStation owns the launcher hotkeys, while DuckStation keeps its own
  // controller mapping. Do not overwrite Pad1 or its controller type here.
  static constexpr struct {
    const char* source;
    const char* target;
  } kHotkeys[] = {
    {"ps1.hotkey.menu.pad", "OpenPauseMenu"}, {"ps1.hotkey.pause.pad", "TogglePause"},
    {"ps1.hotkey.mute.pad", "AudioMute"},     {"ps1.hotkey.quicksave.pad", "SaveSelectedSaveState"},
    {"ps1.hotkey.quickload.pad", "LoadSelectedSaveState"}, {"ps1.hotkey.screenshot.pad", "Screenshot"},
    {"ps1.handle.fastforward", "FastForward"}, {"ps1.handle.rewind", "Rewind"},
  };
  for (const auto& hotkey : kHotkeys)
    SetBinding(settings, values, hotkey.source, "Hotkeys", hotkey.target);
}

} // namespace GBAStationConfig
