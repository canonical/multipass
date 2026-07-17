/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

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

pub fn log_message(level: ffi::Level, category: String, message: String) {
    match ffi::log_message(level, category, message) {
        Ok(()) => (),
        Err(e) => {
            println!("Log message exception: {e}");
        }
    };
}
