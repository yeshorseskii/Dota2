cmake_minimum_required(VERSION 3.16)
project(engine_game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# raylib: use an installed copy, else fetch and build 5.5 automatically.
find_package(raylib 5.0 QUIET)
if(NOT raylib_FOUND)
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5)
  FetchContent_MakeAvailable(raylib)
endif()

# --- engine: reusable, game-agnostic core ---
add_library(engine STATIC
  engine/World.cpp
  engine/ConVar.cpp
  engine/Engine.cpp
  engine/Scene.cpp
)
target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(engine PUBLIC raylib)

# --- game: the MOBA module + entry point ---
add_executable(mini_moba_engine
  src/main.cpp
  game/Game.cpp
  game/Defs.cpp
  third_party/raygui_impl.cpp
)
target_link_libraries(mini_moba_engine PRIVATE engine raylib)

if(UNIX AND NOT APPLE)
  target_link_libraries(engine PUBLIC GL m pthread dl rt X11)
endif()
if(APPLE)
  target_link_libraries(engine PUBLIC
    "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# Copy the character data next to the binary so it loads (and Save writes) there.
add_custom_command(TARGET mini_moba_engine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/data/characters.txt"
    "$<TARGET_FILE_DIR:mini_moba_engine>/characters.txt")
