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
 * Authored by: Antoni Bertolin <antoni.monferrer@canonical.com>
 *
 */

use std::os::raw::c_char;

mod petname_error;
mod petname_generator;

#[cxx::bridge(namespace = "multipass::petname")]
pub mod ffi {
    pub enum NumWords {
        One,
        Two,
        Three,
    }

    extern "Rust" {
        fn generate_petname(n_w: NumWords, sep: c_char) -> Result<String>;
    }
}

fn generate_petname(
    n_w: ffi::NumWords,
    sep: c_char,
) -> Result<String, petname_error::PetnameError> {
    Ok(String::from("curious-scoundrel"))
}
