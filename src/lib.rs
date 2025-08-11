// Main library entry point for multipass Rust modules
pub mod petname;

// Future modules will be added here as they get oxidized, e.g.:
// pub mod utils;
// pub mod network;
// etc.

// Type aliases for FFI bridge
pub type Petname = petname::Petname;

// Centralized FFI bridge that imports from all modules
#[cxx::bridge(namespace = "multipass")]
mod ffi {
    extern "Rust" {
        // Petname module FFI
        type Petname;
        fn new_petname(num_words: i32, separator: &str) -> Box<Petname>;
        fn make_name(petname: &mut Petname) -> String;

        // Petname test functions
        fn test_generates_the_requested_num_words() -> bool;
        fn test_uses_custom_separator() -> bool;
        fn test_generates_two_tokens_by_default() -> bool;
        fn test_can_generate_at_least_hundred_unique_names() -> bool;
        fn test_ffi_integration_test() -> bool;

        // Future module FFI will be added here, e.g.:
        // Utils module FFI
        // fn format_bytes(bytes: u64) -> String;
        // Network module FFI
        // fn validate_ip_address(ip: &str) -> bool;
    }
}

// Re-export FFI wrapper functions from petname module
pub use petname::{
    new_petname, make_name,
    test_generates_the_requested_num_words,
    test_uses_custom_separator,
    test_generates_two_tokens_by_default,
    test_can_generate_at_least_hundred_unique_names,
    test_ffi_integration_test
};
