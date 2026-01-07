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
use rand::prelude::IndexedRandom;
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
        let petname = match self.num_words {
            NumWords::One => choose_from_str_array(NOUNS)?.to_string(),
            NumWords::Two => {
                format!(
                    "{}{}{}",
                    choose_from_str_array(ADJECTIVES)?,
                    self.separator,
                    choose_from_str_array(NOUNS)?
                )
            }
            NumWords::Three => {
                format!(
                    "{}{}{}{}{}",
                    choose_from_str_array(ADVERBS)?,
                    self.separator,
                    choose_from_str_array(ADJECTIVES)?,
                    self.separator,
                    choose_from_str_array(NOUNS)?
                )
            }
            num => return Err(PetnameError::InternalStateError(num.repr)),
        };
        Ok(petname)
    }
}
pub fn make_petname_generator(
    num_words: NumWords,
    separator: c_char,
) -> Result<Box<PetnameGenerator>, PetnameError> {
    match num_words {
        NumWords::One | NumWords::Two | NumWords::Three => {}
        _ => return Err(PetnameError::InvalidWordNumber(num_words.repr)),
    };
    match separator {
        0.. => {}
        _ => return Err(PetnameError::InvalidSeparator(separator)),
    };
    let separator = match separator as u8 as char {
        '-' | '_' | '$' => separator,
        _ => return Err(PetnameError::InvalidSeparator(separator)),
    };

    Ok(Box::new(PetnameGenerator::new(
        num_words,
        separator as u8 as char,
    )))
}
fn choose_from_str_array(word_array: &[&'static str]) -> Result<&'static str, PetnameError> {
    //What is obtained here is a handle to the thread-local RNG. Only initialized the first
    //time it is used in a thread. Subsequent uses only obtain the handle.
    let mut rng = rand::rng();
    word_array
        .choose(&mut rng)
        .copied()
        .ok_or(PetnameError::RNGError)
}
