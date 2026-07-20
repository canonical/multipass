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

use crate::ffi;

pub fn log_message(level: ffi::Level, category: String, message: String) {
    match ffi::log_message(level, category, message) {
        Ok(()) => (),
        Err(e) => {
            println!("Log message exception: {e}");
        }
    };
}

pub fn log_message_with_location(
    level: ffi::Level,
    category: String,
    file: &str,
    line: u32,
    module: &str,
    message: String,
) {
    // Equivalent to detail::extract_filename: gets just the file name from the path
    let filename = file
        .split('/')
        .next_back()
        .unwrap_or(file)
        .split('\\')
        .next_back()
        .unwrap_or(file);

    // Format the location info exactly like the C++ counterpart
    let location_message = format!("{}:{}, `{}`: {}", filename, line, module, message);

    log_message(level, category, location_message);
}

#[macro_export]
macro_rules! error {
    ($category:expr, $($arg:tt)+) => {
        $crate::log_message(
            ffi::Level::Error,
            $category.to_string(),
            format!($($arg)+),
        )
    };
}

#[macro_export]
macro_rules! warning {
    ($category:expr, $($arg:tt)+) => {
        $crate::log_message(
            ffi::Level::Warning,
            $category.to_string(),
            format!($($arg)+),
        )
    };
}

#[macro_export]
macro_rules! debug {
    ($category:expr, $($arg:tt)+) => {
        $crate::log_message(
            ffi::Level::Debug,
            $category.to_string(),
            format!($($arg)+),
        )
    };
}

#[macro_export]
macro_rules! trace {
    ($category:expr, $($arg:tt)+) => {
        $crate::log_message(
            ffi::Level::Trace,
            $category.to_string(),
            format!($($arg)+),
        )
    };
}

// --- Location-Aware Macros ---

#[macro_export]
macro_rules! debug_location {
    ($category:expr, $($arg:tt)+) => {
        $crate::log_message_with_location(
            ffi::Level::Debug,
            $category.to_string(),
            file!(),
            line!(),
            module_path!(),
            format!($($arg)+),
        )
    };
}

#[macro_export]
macro_rules! trace_location {
    ($category:expr, $($arg:tt)+) => {
        $crate::log_message_with_location(
            ffi::Level::Trace,
            $category.to_string(),
            file!(),
            line!(),
            module_path!(),
            format!($($arg)+),
        )
    };
}
