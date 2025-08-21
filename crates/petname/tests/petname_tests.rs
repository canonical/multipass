use multipass_petname::*;
use std::collections::HashSet;

fn split_name<'a>(name: &'a str, separator: &str) -> Vec<&'a str> {
    name.split(separator).collect()
}

#[test]
fn generates_the_requested_num_words() {
    let mut gen1 = new_petname(1, "-").expect("Should create valid petname");
    let mut gen2 = new_petname(2, "-").expect("Should create valid petname");
    let mut gen3 = new_petname(3, "-").expect("Should create valid petname");

    let one_word_name = make_name(&mut gen1).expect("Should generate name");
    let tokens = split_name(&one_word_name, "-");
    assert_eq!(tokens.len(), 1);

    let two_word_name = make_name(&mut gen2).expect("Should generate name");
    let tokens = split_name(&two_word_name, "-");
    assert_eq!(tokens.len(), 2);

    let three_word_name = make_name(&mut gen3).expect("Should generate name");
    let tokens = split_name(&three_word_name, "-");
    assert_eq!(tokens.len(), 3);
}

#[test]
fn uses_custom_separator() {
    let mut name_generator = new_petname(3, "_").expect("Should create valid petname");
    let name = make_name(&mut name_generator).expect("Should generate name");
    let tokens = split_name(&name, "_");
    assert_eq!(tokens.len(), 3);
    assert!(name.contains("_"));
}

#[test]
fn generates_two_tokens_by_default() {
    let mut name_generator = new_petname(2, "-").expect("Should create valid petname");
    let name = make_name(&mut name_generator).expect("Should generate name");
    let tokens = split_name(&name, "-");
    assert_eq!(tokens.len(), 2);

    // Each token should be unique
    let unique_tokens: HashSet<_> = tokens.iter().collect();
    assert_eq!(unique_tokens.len(), tokens.len());
}

#[test]
fn can_generate_at_least_hundred_unique_names() {
    let mut name_generator = new_petname(3, "-").expect("Should create valid petname");
    let mut name_set = HashSet::new();
    let expected_num_unique_names = 100;

    // Generate many names to test uniqueness
    for _ in 0..(10 * expected_num_unique_names) {
        let name = make_name(&mut name_generator).expect("Should generate name");
        name_set.insert(name);
    }

    assert!(name_set.len() >= expected_num_unique_names);
}

#[test]
fn ffi_integration_test() {
    let mut petname = new_petname(2, "-").expect("Should create valid petname");
    let name = make_name(&mut petname).expect("Should generate name");
    assert!(name.contains("-"));
    assert!(!name.is_empty());
}

// error path tests
#[test]
fn new_petname_rejects_invalid_word_counts() {
    assert!(new_petname(0, "-").is_err());
    assert!(new_petname(4, "-").is_err());
    assert!(new_petname(-1, "-").is_err());
    assert!(new_petname(100, "-").is_err());
}

#[test]
fn new_petname_returns_specific_error_for_invalid_counts() {
    let result = new_petname(0, "-");
    assert!(result.is_err());
    let error = result.unwrap_err();
    assert!(matches!(error, PetnameError::InvalidNumWords(0)));

    let result = new_petname(5, "-");
    assert!(result.is_err());
    let error = result.unwrap_err();
    assert!(matches!(error, PetnameError::InvalidNumWords(5)));
}

#[test]
fn error_display_formatting() {
    let error = PetnameError::InvalidNumWords(42);
    assert_eq!(
        error.to_string(),
        "Invalid number of words: 42. Must be 1, 2, or 3"
    );

    let error = PetnameError::EmptyWordList("test".to_string());
    assert_eq!(error.to_string(), "Empty word list: test");
}
