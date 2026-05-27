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

pub mod petname_error;
pub mod petname_generator;
use petname_generator::PetnameBackend;

#[cxx::bridge(namespace = "multipass::petname")]
pub mod ffi {
    //Both the stated here and in extern C++ tells the CXX bridge generator
    //to use the C++-side definition of NumWords
    #[repr(i32)]
    pub enum NumWords {
        One,
        Two,
        Three,
    }

    #[namespace = "rxx::petname"]
    extern "Rust" {
        type PetnameBackend;
        fn make_name(self: &mut PetnameBackend) -> String;
        #[Self = "PetnameBackend"]
        fn make_petname_backend(
            num_words: NumWords,
            separator: c_char,
            seed: u64,
        ) -> Result<Box<PetnameBackend>>;
        //Result<T> allows CXX to turn returned Rust Errors into cxx::RustError
        //on the C++ side.
    }
    #[namespace = "multipass::petname"]
    extern "C++" {
        include!("multipass/name_generator.h");
        type NumWords;
    }
}
