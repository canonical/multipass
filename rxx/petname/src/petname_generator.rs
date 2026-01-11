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

#[cfg(test)]
mod tests {
    use std::collections::HashSet;

    use super::*;
    #[test]
    fn generates_requested_word_number() {
        let petname_1 = PetnameGenerator::new(NumWords::One, '-')
            .make_name()
            .unwrap();
        let petname_2 = PetnameGenerator::new(NumWords::Two, '-')
            .make_name()
            .unwrap();
        let petname_3 = PetnameGenerator::new(NumWords::Three, '-')
            .make_name()
            .unwrap();
        assert_eq!(petname_1.split('-').count(), 1);
        assert_eq!(petname_2.split('-').count(), 2);
        assert_eq!(petname_3.split('-').count(), 3);
    }
    #[test]
    fn filters_out_bad_input() {
        //First we test that failure is not due to CXX enum syntax
        let result = make_petname_generator(NumWords { repr: 0 }, '-' as i8);
        assert!(result.is_ok());
        let result = make_petname_generator(NumWords { repr: 4 }, '-' as i8);
        assert!(matches!(result, Err(PetnameError::InvalidWordNumber(4))));

        let result = make_petname_generator(NumWords::One, '(' as i8);
        assert!(matches!(result, Err(PetnameError::InvalidSeparator(_))));
    }
    #[test]
    fn can_generate_unique_names() {
        let mut hashset: HashSet<String> = HashSet::new();
        let petname_generator = PetnameGenerator::new(NumWords::Three, '-');
        const TOTAL_NAMES: usize = 1000;

        for i in 0..TOTAL_NAMES {
            let petname = petname_generator.make_name().unwrap();
            assert!(
                hashset.insert(petname),
                "Generated duplicate petname at iteration {}",
                i
            );
        }
    }
    #[test]
    fn choose_fails_on_empty() {
        assert_eq!(choose_from_str_array(&[]), Err(PetnameError::RNGError));
    }
    #[test]
    fn error_on_impossible_petname_state() {
        let mut petname_generator = PetnameGenerator::new(NumWords::Two, '-');

        let bad_num_words = 4;
        petname_generator.num_words = NumWords {
            repr: bad_num_words,
        };
        let result = petname_generator.make_name();
        assert_eq!(result, Err(PetnameError::InternalStateError(bad_num_words)));
    }
}
