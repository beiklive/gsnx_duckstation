// SPDX-FileCopyrightText: 2026 GBAStation contributors
// SPDX-License-Identifier: GPL-3.0 OR CC-BY-NC-ND-4.0

#pragma once

#include <cstdint>
#include <string>

namespace GBAStationGameDB {

struct Entry
{
  std::string path;
  std::string title;
  std::string save_path;
  std::int64_t play_count = 0;
  std::int64_t play_time = 0;
  std::string last_played;
};

bool LoadForPath(const std::string& rom_path, Entry* entry);
void OnGameStarted(const std::string& rom_path);
void OnGameStopped();

} // namespace GBAStationGameDB
