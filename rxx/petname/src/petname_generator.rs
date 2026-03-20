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

use crate::ffi::NumWords;
use crate::petname_error::PetnameError;

use macros::make_word_array;
use rand::{prelude::IndexedRandom, rngs::ThreadRng};
use static_assertions as sa;

use std::os::raw::c_char;
//Paths are relative to the workspace root
const ADJECTIVES: &[&str] = &make_word_array!("petname/src/adjectives.txt");
const ADVERBS: &[&str] = &make_word_array!("petname/src/adverbs.txt");
const NOUNS: &[&str] = &make_word_array!("petname/src/names.txt");

sa::const_assert!(!NOUNS.is_empty());
sa::const_assert!(!ADJECTIVES.is_empty());
sa::const_assert!(!ADVERBS.is_empty());

pub fn make_petname(num_words: NumWords, separator_8: c_char) -> Result<String, PetnameError> {
    let separator_c = separator_8 as u8 as char;
    let separator_c: char = match separator_c {
        '-' | '_' => separator_c,
        _ => return Err(PetnameError::InvalidSeparator(separator_8 as i8)),
    };

    let sources: &[&[&str]] = match num_words {
        NumWords::One => &[NOUNS],
        NumWords::Two => &[ADJECTIVES, NOUNS],
        NumWords::Three => &[ADVERBS, ADJECTIVES, NOUNS],
        num => return Err(PetnameError::InvalidWordNumber(num.repr)),
    };

    let mut rng_engine = rand::rng();

    let words: Vec<_> = sources
        .iter()
        .map(|&arr| choose_from_str_array(arr, &mut rng_engine))
        .collect();

    Ok(words.join(&separator_c.to_string()))
}
fn choose_from_str_array(word_array: &[&'static str], rng: &mut ThreadRng) -> &'static str {
    match word_array.choose(rng).copied() {
        Some(str) => str,
        //If one of the sources of words was empty, compile-time failure
        _ => unreachable!(),
    }
}
