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
 * Authored by: Antoni Bertolin Monferrer <antoni.monferrer@canonical.com>
 *
 */

mod petname_error;
mod petname_generator;
use petname_generator::{PetnameGenerator, make_petname_generator};

#[cxx::bridge(namespace = "multipass::petname")]
pub mod ffi {
    #[repr(i32)]
    pub enum NumWords {
        One,
        Two,
        Three,
    }

    #[namespace = "rxx::petname"]
    extern "Rust" {
        type PetnameGenerator;
        fn make_petname_generator(
            num_words: NumWords,
            separator: c_char,
        ) -> Result<Box<PetnameGenerator>>;
        fn make_name(self: &PetnameGenerator) -> Result<String>;
    }
    #[namespace = "multipass::petname"]
    extern "C++" {
        include!("multipass/petname_interface.h");
        type NumWords;
    }
}
