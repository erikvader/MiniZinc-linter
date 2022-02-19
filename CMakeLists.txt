cmake_minimum_required(VERSION 3.16)
project(MiniZincLinter
  LANGUAGES CXX
  DESCRIPTION "I am a description"
  VERSION 1.2.3)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Activate my custom flags for development.
option(LZN_FLAGS "Whether to activate my development flags" OFF)
# Flags for everything:
if(LZN_FLAGS)
  set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -Og")
endif()

# Flags for my files
macro(common_configuration)
set(CMAKE_CXX_STANDARD 17)
if(LZN_FLAGS)
  set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic")
endif()
endmacro()

# Set the default back to false
# https://github.com/MiniZinc/libminizinc/blob/e385f96afad3c1c3a361a0378e7e24898dcef14d/CMakeLists.txt#L30
option(CMAKE_POSITION_INDEPENDENT_CODE "Default value for POSITION_INDEPENDENT_CODE of targets" FALSE)

# Include my targets
add_subdirectory(src)
target_include_directories(LinterLib SYSTEM PUBLIC deps/rang/include)

# Include MiniZinc targets
add_subdirectory(deps/libminizinc EXCLUDE_FROM_ALL)
target_link_libraries(LinterLib PUBLIC mzn)
#NOTE: include_directories from mzn were not public
target_include_directories(
  LinterLib
  SYSTEM
  PUBLIC
  $<TARGET_PROPERTY:mzn,INCLUDE_DIRECTORIES>
  )

# Tests
add_subdirectory(tests)
add_subdirectory(deps/Catch2 EXCLUDE_FROM_ALL)
target_link_libraries(Test PRIVATE Catch2::Catch2)

# Install
install(TARGETS lzn)

# makepkg
set(PACMAN_PKG_EXT ".tar.zst" CACHE STRING "extension to use when packaging for pacman")
set(PACMAN_INSTALL_DIR "install")
set(PACMAN_INSTALL_FILE "lzn.install")
set(PACMAN_LIGHT "makepkg-light.sh")
set(PACMAN_PKG_DEST "${PROJECT_NAME}-${PROJECT_VERSION}-0-x86_64.pkg${PACMAN_PKG_EXT}")

configure_file("${PACMAN_INSTALL_FILE}.in" "${PACMAN_INSTALL_FILE}" @ONLY)
configure_file("${PACMAN_LIGHT}.in" "${PACMAN_LIGHT}" @ONLY)

add_custom_target(
  package
  "./${PACMAN_LIGHT}"
  BYPRODUCTS
  "${PACMAN_INSTALL_DIR}"
  "${PACMAN_PKG_DEST}")
