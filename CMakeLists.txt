cmake_minimum_required(VERSION 3.19) #TODO: probably to high
project(MiniZincLinter)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)

add_subdirectory(deps/libminizinc EXCLUDE_FROM_ALL)
target_link_libraries(LinterLib PUBLIC mzn)
#NOTE: include_directories from mzn were not public
target_include_directories(
  LinterLib
  PUBLIC
  $<TARGET_PROPERTY:mzn,INCLUDE_DIRECTORIES>
  )

#TODO: do tests like this?
enable_testing()
add_subdirectory(tests)
add_test(NAME MyTest COMMAND Test)
