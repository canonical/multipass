#[cxx::bridge(namespace = "logger")]
pub mod ffi {
    #[namespace = "multipass::logging"]
    #[repr(i32)]
    enum Level {
        error,
        warning,
        info,
        debug,
        trace,
    }

    #[namespace = "multipass::logging"]
    unsafe extern "C++" {
        include!("multipass/logging/level.h");
        type Level;
        //#[cxx_name = "Level"]
        //fn level_from(in_: i32) -> Level;
        include!("multipass/logging/log.h");
        #[namespace = "multipass::logging::rust"]
        fn log_message(level: Level, category: String, message: String) -> Result<()>;
        //Result<1Type> in a CXX bridge unsafe extern C++ will convert to a try-catch in the C++
        //side which will convert the C++ exception to a rust error.
    }
}

fn log_message(level: ffi::Level, category: String, message: String) {
    match ffi::log_message(level, category, message) {
        Ok(()) => (),
        Err(e) => {
            println!("Log message exception: {e}");
        }
    };
}
pub fn log_error(category: String, message: String) {
    log_message(ffi::Level::error, category, message)
}
