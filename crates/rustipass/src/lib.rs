pub mod petname;

// Re-export petname public API for FFI
pub use petname::{make_name, new_petname, Petname};

// CXX FFI Bridge for petname module
#[cxx::bridge(namespace = "multipass::petname")]
mod petname_ffi {
    extern "Rust" {
        type Petname;
        fn new_petname(num_words: i32, separator: &str) -> Result<Box<Petname>>;
        fn make_name(petname: &mut Petname) -> Result<String>;
    }
}

// Future modules will have their own FFI bridges:
// #[cxx::bridge(namespace = "multipass::utils")]
// mod utils_ffi {
//     extern "Rust" {
//         // utils functions here
//     }
// }

// #[cxx::bridge(namespace = "multipass::network")]
// mod network_ffi {
//     extern "Rust" {
//         // network functions here
//     }
// }
