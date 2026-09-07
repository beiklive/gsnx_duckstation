// SPDX-FileCopyrightText: 2026 GBAStation contributors
// SPDX-License-Identifier: GPL-3.0 OR CC-BY-NC-ND-4.0

#pragma once

#include <string_view>

namespace GBAStation::SwitchPaths {

inline constexpr std::string_view ReturnNro = "sdmc:/switch/GBAStation.nro";
inline constexpr std::string_view DataRoot = "sdmc:/GBAStation/duckstation";
inline constexpr std::string_view ConfigFile = "sdmc:/GBAStation/config/config.cfg";
inline constexpr std::string_view LegacyConfigFile = "/GBAStation/config/config.cfg";
inline constexpr std::string_view GameDatabaseFile = "sdmc:/GBAStation/data/GameData_PS1.json";
inline constexpr std::string_view LegacyGameDatabaseFile = "/GBAStation/data/GameData_PS1.json";
inline constexpr std::string_view RomfsResources = "romfs:/resources";

inline constexpr std::string_view PortableMarker = "portable.txt";
inline constexpr std::string_view SettingsFile = "settings.ini";
inline constexpr std::string_view LogFile = "duckstation.log";

} // namespace GBAStation::SwitchPaths
