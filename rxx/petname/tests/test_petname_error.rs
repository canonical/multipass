use petname::petname_error::PetnameError;
#[test]
fn test_error_display() {
    assert_eq!(
        PetnameError::InvalidWordNumber(5).to_string(),
        "Invalid word number: 5"
    );
    assert_eq!(
        PetnameError::InvalidSeparator(64).to_string(),
        "Invalid separator, ASCII code: 64"
    );
    assert_eq!(
        PetnameError::EmptyArrayError.to_string(),
        "Cannot choose an element of an empty array"
    );
    assert_eq!(
        PetnameError::InternalStateError(10).to_string(),
        "Invalid internal state: numWords = 10"
    );
}
