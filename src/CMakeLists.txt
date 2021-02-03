#TODO: find a better name maybe
add_library(LinterLib STATIC)
target_include_directories(LinterLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(LinterLib PUBLIC -Wall -Wextra -pedantic)

add_executable(lzn)
target_sources(lzn PRIVATE main.cpp)
target_link_libraries(lzn PRIVATE LinterLib)
set_target_properties(lzn PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(linter)
