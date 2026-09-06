// SPDX-FileCopyrightText: 2026 GBAStation contributors
// SPDX-License-Identifier: GPL-3.0 OR CC-BY-NC-ND-4.0

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

class SettingsInterface;

namespace GBAStationConfig {

using Values = std::unordered_map<std::string, std::string>;

bool Load(Values* values, std::string* loaded_path = nullptr);

// Copies the PS1 core settings from config.cfg into DuckStation's native
// SettingsInterface sections. Legacy ps1.* core keys are accepted as a
// fallback. Input mappings remain entirely in DuckStation's native settings.
void ApplyCoreSettings(const Values& values, SettingsInterface& settings);

} // namespace GBAStationConfig
