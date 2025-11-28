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
        fn log_message(level: Level, category: String, message: String);
    }
}

pub fn log_message(level: ffi::Level, category: String, message: String) {
    ffi::log_message(level, category, message);
}
