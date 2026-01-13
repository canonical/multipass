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

pub struct PetnameGenerator {
    num_words: NumWords,
    separator: char,
}

impl PetnameGenerator {
    fn new(num_words: NumWords, separator: char) -> Self {
        PetnameGenerator {
            num_words,
            separator,
        }
    }
    pub fn make_name(&self) -> Result<String, PetnameError> {
        let mut rng_engine = rand::rng();

        let sources: &[&[&str]] = match self.num_words {
            NumWords::One => &[NOUNS],
            NumWords::Two => &[ADJECTIVES, NOUNS],
            NumWords::Three => &[ADVERBS, ADJECTIVES, NOUNS],
            num => return Err(PetnameError::InternalStateError(num.repr)),
        };

        let words: Result<Vec<_>, _> = sources
            .iter()
            .map(|&arr| choose_from_str_array(arr, &mut rng_engine))
            .collect();

        Ok(words?.join(&self.separator.to_string()))
    }
}
pub fn make_petname_generator(
    num_words: NumWords,
    separator_i8: c_char,
) -> Result<Box<PetnameGenerator>, PetnameError> {
    match num_words {
        NumWords::One | NumWords::Two | NumWords::Three => {}
        _ => return Err(PetnameError::InvalidWordNumber(num_words.repr)),
    };
    let separator_c = separator_i8 as u8 as char;
    let separator_c: char = match separator_c {
        '-' | '_' | '$' => separator_c,
        _ => return Err(PetnameError::InvalidSeparator(separator_i8)),
    };

    Ok(Box::new(PetnameGenerator::new(num_words, separator_c)))
}
fn choose_from_str_array(
    word_array: &[&'static str],
    rng: &mut ThreadRng,
) -> Result<&'static str, PetnameError> {
    word_array
        .choose(rng)
        .copied()
        .ok_or(PetnameError::EmptyArrayError)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn choose_fails_on_empty() {
        //The intention of this test is that if the function is called on an empty array it must
        //not panic, but return an error instead.
        assert_eq!(
            choose_from_str_array(&[], &mut rand::rng()),
            Err(PetnameError::EmptyArrayError)
        );
    }
    #[test]
    fn error_on_bad_internal_petname_state() {
        //The intention of this test is that on a wrong internal state, make_name must not panic,
        //but return an error instead.
        let mut petname_generator = PetnameGenerator::new(NumWords::Two, '-');

        let bad_num_words = 4;
        petname_generator.num_words = NumWords {
            repr: bad_num_words,
        };
        let result = petname_generator.make_name();
        assert_eq!(result, Err(PetnameError::InternalStateError(bad_num_words)));
    }
}
