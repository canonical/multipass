pub mod petname;

// Re-export petname public API for FFI
pub use petname::{make_name, new_petname, Petname};

// CXX FFI Bridge for all rustipass modules
#[cxx::bridge(namespace = "multipass::petname")]
mod ffi {
    extern "Rust" {
        type Petname;
        fn new_petname(num_words: i32, separator: &str) -> Result<Box<Petname>>;
        fn make_name(petname: &mut Petname) -> Result<String>;
    }
}
