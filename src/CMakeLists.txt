common_configuration()

#TODO: find a better name maybe
add_library(LinterLib OBJECT)
target_include_directories(LinterLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(LinterLib SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(lzn)
target_sources(lzn PRIVATE main.cpp)
target_link_libraries(lzn PRIVATE LinterLib)
set_target_properties(lzn PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(linter)
