cmake_minimum_required(VERSION 3.19) #TODO: probably to high
project(MiniZincLinter CXX)

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
