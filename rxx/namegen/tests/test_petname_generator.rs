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

use namegen::{ffi::NumWords, petname_error::PetnameError, petname_generator::PetnameBackend};
use std::collections::HashSet;
use std::ffi::c_char;

#[test]
fn generates_requested_word_number_with_separator() {
    let petname_1 = PetnameBackend::make_petname_backend(NumWords::One, '-' as c_char, 0)
        .unwrap()
        .make_name();
    let petname_2 = PetnameBackend::make_petname_backend(NumWords::Two, '-' as c_char, 0)
        .unwrap()
        .make_name();
    let petname_3 = PetnameBackend::make_petname_backend(NumWords::Three, '-' as c_char, 0)
        .unwrap()
        .make_name();
    assert_eq!(petname_1.split('-').count(), 1);
    assert_eq!(petname_2.split('-').count(), 2);
    assert_eq!(petname_3.split('-').count(), 3);
}

#[test]
fn filters_out_bad_input() {
    //First we test that failure is not due to CXX enum syntax
    let result = PetnameBackend::make_petname_backend(NumWords { repr: 0 }, '-' as c_char, 0);
    assert!(result.is_ok());
    let result = PetnameBackend::make_petname_backend(NumWords { repr: 4 }, '-' as c_char, 0);
    assert!(matches!(result, Err(PetnameError::InvalidWordNumber(4))));

    let result = PetnameBackend::make_petname_backend(NumWords::One, '(' as c_char, 0);
    assert!(matches!(result, Err(PetnameError::InvalidSeparator(_))));
}

#[test]
fn can_generate_unique_names() {
    let mut hashset: HashSet<String> = HashSet::new();
    const TOTAL_NAMES: usize = 1000;

    let mut petname_backend =
        PetnameBackend::make_petname_backend(NumWords::Three, '-' as c_char, 0).unwrap();

    for i in 0..TOTAL_NAMES {
        let petname = petname_backend.make_name();
        assert!(
            hashset.insert(petname),
            "Generated duplicate petname at iteration {}",
            i
        );
    }
}
