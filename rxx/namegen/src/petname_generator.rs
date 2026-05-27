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

use crate::ffi::NumWords;
use crate::petname_error::PetnameError;

use macros::make_word_array;
use rand::{SeedableRng, prelude::IndexedRandom, rngs::SmallRng};
use static_assertions as sa;

use std::os::raw::c_char;
//Paths are relative to the workspace root
const ADJECTIVES: &[&str] = &make_word_array!("namegen/assets/adjectives.txt");
const ADVERBS: &[&str] = &make_word_array!("namegen/assets/adverbs.txt");
const NOUNS: &[&str] = &make_word_array!("namegen/assets/names.txt");

sa::const_assert!(!NOUNS.is_empty());
sa::const_assert!(!ADJECTIVES.is_empty());
sa::const_assert!(!ADVERBS.is_empty());

pub struct PetnameBackend {
    num_words: NumWords,
    separator: char,
    rng: SmallRng,
}

impl PetnameBackend {
    pub fn make_petname_backend(
        num_words: NumWords,
        separator_c: c_char,
        seed: u64,
    ) -> Result<Box<Self>, PetnameError> {
        let separator = separator_c as u8 as char;
        if !matches!(separator, '-' | '_') {
            return Err(PetnameError::InvalidSeparator(separator_c as i8));
        }
        if !matches!(num_words, NumWords::One | NumWords::Two | NumWords::Three) {
            return Err(PetnameError::InvalidWordNumber(num_words.repr));
        };
        Ok(Box::new(Self {
            num_words,
            separator,
            rng: SmallRng::seed_from_u64(seed),
        }))
    }
    pub fn make_name(&mut self) -> String {
        let sources: &[&[&str]] = match self.num_words {
            NumWords::One => &[NOUNS],
            NumWords::Two => &[ADJECTIVES, NOUNS],
            NumWords::Three => &[ADVERBS, ADJECTIVES, NOUNS],
            //Constructor should have picked up the error
            _ => unreachable!(),
        };

        let words: Vec<_> = sources
            .iter()
            .map(|&arr| choose_from_str_array(arr, &mut self.rng))
            .collect();

        words.join(&self.separator.to_string())
    }
}

fn choose_from_str_array(word_array: &[&'static str], rng: &mut SmallRng) -> &'static str {
    match word_array.choose(rng).copied() {
        Some(str) => str,
        //If one of the sources of words was empty, compile-time failure
        _ => unreachable!(),
    }
}
