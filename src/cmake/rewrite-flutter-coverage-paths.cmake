# Rewrites the package-relative paths that `flutter test --coverage` emits
# (e.g. SF:lib/main.dart) into full source-tree paths so the Dart entries line
# up with the repo layout when merged with the C++/Rust coverage.
#
# Portable across platforms (uses CMake instead of sed/perl), invoked as:
#   cmake -DLCOV_FILE=<path> -DSOURCE_PREFIX=<dir> -P rewrite-flutter-coverage-paths.cmake

if(NOT DEFINED LCOV_FILE OR NOT DEFINED SOURCE_PREFIX)
  message(FATAL_ERROR "LCOV_FILE and SOURCE_PREFIX must be defined")
endif()

file(READ "${LCOV_FILE}" _contents)
string(REGEX REPLACE "(^|\n)SF:lib/" "\\1SF:${SOURCE_PREFIX}/lib/" _contents "${_contents}")
file(WRITE "${LCOV_FILE}" "${_contents}")
