cmake_minimum_required(VERSION 3.16)
project(mini_moba_3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Prefer an installed raylib (find_package). If it isn't found, fetch and build
# raylib 5.5 automatically so `cmake --build` works from a clean machine.
find_package(raylib 5.0 QUIET)
if(NOT raylib_FOUND)
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(FETCHCONTENT_QUIET OFF)
  FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
  )
  FetchContent_MakeAvailable(raylib)
endif()

add_executable(mini_moba_3d src/main.cpp)
target_link_libraries(mini_moba_3d PRIVATE raylib)

# raylib needs these system libs on Linux when linked statically.
if(UNIX AND NOT APPLE)
  target_link_libraries(mini_moba_3d PRIVATE GL m pthread dl rt X11)
endif()
if(APPLE)
  target_link_libraries(mini_moba_3d PRIVATE
    "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
