
#include <petnamers/src/lib.rs.h>
#include <rust/cxx.h>
// The purpose of this header is to act as 1: a prettyfier of the included header in source and 2:
// to declare all wrappers for the Rust code, which should always include a try catch with rethrow
// plus argument checking with throw if incorrect. Exception type has to be rust::Error because that
// type is declared as final and the caller of the wrapped function will expect that exception but
// could forget about a non-rust::Error-derived exception.
