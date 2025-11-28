use std::os::raw::c_char;
#[cxx::bridge(namespace = "petnamers")]
pub mod ffi {
    pub enum NumWords {
        One,
        Two,
        Three,
    }

    extern "Rust" {
        fn generate_petname(n_w: NumWords, sep: c_char) -> String;
    }
    #[namespace = "multipass::logging"]
    extern "C++" {
        type Level = rust_logger::ffi::Level;
    }
}
//use cxx::{ExternType, type_id};
//
//unsafe impl cxx::ExternType for ffi::Level {
//    type Id = cxx::type_id!("multipass::logging::Level");
//    type Kind = cxx::kind::Trivial; // enum is trivially copyable
//}

fn generate_petname(n_w: ffi::NumWords, sep: c_char) -> String {
    rust_logger::log_message(
        ffi::Level::error,
        String::from("rust error"),
        String::from("You found out!"),
    );
    String::from("curious-scoundrel")
}
