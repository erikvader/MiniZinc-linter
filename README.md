# About
This is my master thesis project for a
[linter](https://en.wikipedia.org/wiki/Lint_(software)) for
[MiniZinc](https://www.minizinc.org/).

# Linting rules
Each linting rule is a separate issue tagged with `lint`. This gives
each one a unique id and space for discussions on a specific rule.
Some are labeled with `help wanted`, which means I have a question or
have something I don't understand about a rule. Feel free to add more
linting rules and to comment with opinions.

# Issue
Have something to discuss? Create an issue!

# Build instructions
```sh
mkdir build
cd build
cmake ..
cmake --build .
```

## Language server
Run the first cmake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=YES` and add
a symbolic link to the root directory with `ln -sr compile_commands.json ..`
to make [ccls](https://github.com/MaskRay/ccls) find all files.
