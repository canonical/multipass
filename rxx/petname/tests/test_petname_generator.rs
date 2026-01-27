use petname::{
    ffi::NumWords, petname_error::PetnameError, petname_generator::make_petname_generator,
};
use std::collections::HashSet;
use std::ffi::c_char;

#[test]
fn generates_requested_word_number_with_separator() {
    let petname_1 = make_petname_generator(NumWords::One, '-' as c_char)
        .unwrap()
        .make_name()
        .unwrap();
    let petname_2 = make_petname_generator(NumWords::Two, '-' as c_char)
        .unwrap()
        .make_name()
        .unwrap();
    let petname_3 = make_petname_generator(NumWords::Three, '-' as c_char)
        .unwrap()
        .make_name()
        .unwrap();
    assert_eq!(petname_1.split('-').count(), 1);
    assert_eq!(petname_2.split('-').count(), 2);
    assert_eq!(petname_3.split('-').count(), 3);
}

#[test]
fn filters_out_bad_input() {
    //First we test that failure is not due to CXX enum syntax
    let result = make_petname_generator(NumWords { repr: 0 }, '-' as c_char);
    assert!(result.is_ok());
    let result = make_petname_generator(NumWords { repr: 4 }, '-' as c_char);
    assert!(matches!(result, Err(PetnameError::InvalidWordNumber(4))));

    let result = make_petname_generator(NumWords::One, '(' as c_char);
    assert!(matches!(result, Err(PetnameError::InvalidSeparator(_))));
}

#[test]
fn can_generate_unique_names() {
    let mut hashset: HashSet<String> = HashSet::new();
    let petname_generator = make_petname_generator(NumWords::Three, '-' as c_char).unwrap();
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
