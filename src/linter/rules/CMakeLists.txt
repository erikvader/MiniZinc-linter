target_sources(LinterLib PRIVATE
  no-domain-var-decl.cpp
  constant-variable.cpp
  unused-var-funcs.cpp
  var-in-gen.cpp
  var-in-if-where.cpp
  element-predicate.cpp
  symmetry-breaking.cpp
  one-based-arrays.cpp
  compacted-if.cpp
  zero-one-variables.cpp
  globals-in-function.cpp
  global-constraint-reified.cpp
  operators-on-var.cpp
  functionally-defined-search-hint.cpp
  )
