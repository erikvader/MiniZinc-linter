common_configuration()

Include(FetchContent)
message(STATUS "Downloading Catch2...")
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v2.13.4
  )
FetchContent_MakeAvailable(Catch2)
message(STATUS "Downloading Catch2 done")

add_executable(Test test.cpp no-domain-var-decl.test.cpp)
target_link_libraries(Test PRIVATE LinterLib)
target_link_libraries(Test PRIVATE Catch2::Catch2)

add_custom_target(test Test)
