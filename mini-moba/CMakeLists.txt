cmake_minimum_required(VERSION 3.16)
project(mini_moba LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# SFML 2.5+ (Ubuntu ships 2.6). Components: graphics pulls in window + system.
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

add_executable(mini_moba
  src/main.cpp
  src/Game.cpp
  src/Render.cpp
)
target_include_directories(mini_moba PRIVATE src)
target_link_libraries(mini_moba PRIVATE sfml-graphics sfml-window sfml-system)

# Copy any bundled assets (e.g. a font) next to the binary if present.
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
  add_custom_command(TARGET mini_moba POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:mini_moba>/assets")
endif()
