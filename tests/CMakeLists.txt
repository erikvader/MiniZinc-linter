common_configuration()

add_executable(Test test.cpp no-domain-var-decl.test.cpp rustiter.test.cpp)
target_link_libraries(Test PRIVATE LinterLib)

add_custom_target(test Test)
