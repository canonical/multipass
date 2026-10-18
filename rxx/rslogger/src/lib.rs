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

pub mod log;

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
        include!("multipass/logging/log.h");
        #[namespace = "multipass::logging::rust"]
        fn log_message(level: Level, category: String, message: String) -> Result<()>;
    }
    #[namespace = "rxx::test::logging"]
    extern "Rust" {
        //Integration tests purpose only
        fn test_log(level: Level, category: String, message: String) -> Result<()>;
    }
}

pub fn test_log(
    level: ffi::Level,
    category: String,
    message: String,
) -> Result<(), cxx::Exception> {
    ffi::log_message(level, category, message)
}
