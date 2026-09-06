add_executable(duckstation-nogui
  gbastation_config.cpp
  gbastation_config.h
  gbastation_game_db.cpp
  gbastation_game_db.h
  nogui_host.cpp
  nogui_host.h
  nogui_platform.h
  switch_paths.h
)

target_precompile_headers(duckstation-nogui PRIVATE "pch.h")
target_link_libraries(duckstation-nogui PRIVATE core util common imgui scmversion rapidjson)

add_core_resources(duckstation-nogui)

# Reuse DuckStation's maintained Simplified Chinese catalog for the NoGUI
# fullscreen menu. The NoGUI host provides a small Qt-free TS lookup layer.
add_resources(duckstation-nogui
  ${CMAKE_SOURCE_DIR}/src/duckstation-qt/translations/duckstation-qt_zh-CN.ts
  ${CMAKE_SOURCE_DIR}/src/duckstation-qt/translations)

if(NINTENDO_SWITCH)
  enable_language(ASM)

  target_sources(duckstation-nogui PRIVATE
    switch_nogui_platform.cpp
    switch_nogui_platform.h
    switch_asm.S
  )
  target_link_libraries(duckstation-nogui PUBLIC "${NX_LIBRARIES}")

  nx_generate_nacp (GBAStationDuckStationStub.nacp
    NAME    "GBAStation DuckStation Stub"
    AUTHOR  "GBAStation"
    VERSION "pre-release 1"
  )

  # Create folder early otherwise the CMake script complains
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/romfs)

  if (CMAKE_BUILD_TYPE STREQUAL Debug)
    nx_create_nro(duckstation-nogui
      OUTPUT ${CMAKE_BINARY_DIR}/GBAStationDuckStationStub.nro
      NACP  GBAStationDuckStationStub.nacp
      ICON  "${CMAKE_SOURCE_DIR}/extras/icons/icon-switch.jpg"
    )
  else()
    nx_create_nro(duckstation-nogui
      OUTPUT ${CMAKE_BINARY_DIR}/GBAStationDuckStationStub.nro
      NACP  GBAStationDuckStationStub.nacp
      ICON  "${CMAKE_SOURCE_DIR}/extras/icons/icon-switch.jpg"
      ROMFS ${CMAKE_BINARY_DIR}/romfs
    )
  endif()
endif()
