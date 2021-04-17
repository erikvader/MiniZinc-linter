common_configuration()

add_executable(Test
  test.cpp
  no-domain-var-decl.test.cpp
  rustiter.test.cpp
  searcher.test.cpp
  LinterEnv.test.cpp
  constant-variable.test.cpp
  unused-var-funcs.test.cpp
  var-in-gen.test.cpp
  var-in-if-where.test.cpp
  one-based-arrays.test.cpp
  compacted-if.test.cpp
  )
target_link_libraries(Test PRIVATE LinterLib)

add_custom_target(test Test)
