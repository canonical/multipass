use rand::seq::SliceRandom;
use rand::thread_rng;

const ADJECTIVES: &str = include_str!("adjectives.txt");
const ADVERBS: &str = include_str!("adverbs.txt");
const NAMES: &str = include_str!("names.txt");

pub enum NumWords {
    One,
    Two,
    Three,
}

pub struct Petname {
    num_words: NumWords,
    separator: String,
}

impl Petname {
    pub fn new(num_words: NumWords, separator: &str) -> Self {
        Self {
            num_words,
            separator: separator.to_string(),
        }
    }

    pub fn make_name(&self) -> String {
        let mut rng = thread_rng();
        let adjectives: Vec<&str> = ADJECTIVES.lines().collect();
        let adverbs: Vec<&str> = ADVERBS.lines().collect();
        let names: Vec<&str> = NAMES.lines().collect();

        let adjective = adjectives.choose(&mut rng).unwrap();
        let adverb = adverbs.choose(&mut rng).unwrap();
        let name = names.choose(&mut rng).unwrap();

        match self.num_words {
            NumWords::One => name.to_string(),
            NumWords::Two => format!("{}{}{}", adjective, self.separator, name),
            NumWords::Three => format!(
                "{}{}{}{}{}",
                adverb, self.separator, adjective, self.separator, name
            ),
        }
    }
}

pub fn new_petname(num_words: i32, separator: &str) -> Box<Petname> {
    let num_words = match num_words {
        1 => NumWords::One,
        2 => NumWords::Two,
        3 => NumWords::Three,
        _ => panic!("Invalid number of words"),
    };
    Box::new(Petname::new(num_words, separator))
}

pub fn make_name(petname: &mut Petname) -> String {
    petname.make_name()
}

// Test functions exposed to C++
pub fn test_generates_the_requested_num_words() -> bool {
    use std::panic;

    panic::catch_unwind(|| {
        let mut gen1 = new_petname(1, "-");
        let mut gen2 = new_petname(2, "-");
        let mut gen3 = new_petname(3, "-");

        let one_word_name = make_name(&mut gen1);
        let tokens: Vec<&str> = one_word_name.split("-").collect();
        assert_eq!(tokens.len(), 1);

        let two_word_name = make_name(&mut gen2);
        let tokens: Vec<&str> = two_word_name.split("-").collect();
        assert_eq!(tokens.len(), 2);

        let three_word_name = make_name(&mut gen3);
        let tokens: Vec<&str> = three_word_name.split("-").collect();
        assert_eq!(tokens.len(), 3);
    }).is_ok()
}

pub fn test_uses_custom_separator() -> bool {
    use std::panic;

    panic::catch_unwind(|| {
        let mut name_generator = new_petname(3, "_");
        let name = make_name(&mut name_generator);
        let tokens: Vec<&str> = name.split("_").collect();
        assert_eq!(tokens.len(), 3);
        assert!(name.contains("_"));
    }).is_ok()
}

pub fn test_generates_two_tokens_by_default() -> bool {
    use std::panic;
    use std::collections::HashSet;

    panic::catch_unwind(|| {
        let mut name_generator = new_petname(2, "-");
        let name = make_name(&mut name_generator);
        let tokens: Vec<&str> = name.split("-").collect();
        assert_eq!(tokens.len(), 2);

        // Each token should be unique
        let unique_tokens: HashSet<_> = tokens.iter().collect();
        assert_eq!(unique_tokens.len(), tokens.len());
    }).is_ok()
}

pub fn test_can_generate_at_least_hundred_unique_names() -> bool {
    use std::panic;
    use std::collections::HashSet;

    panic::catch_unwind(|| {
        let mut name_generator = new_petname(3, "-");
        let mut name_set = HashSet::new();
        let expected_num_unique_names = 100;

        // Generate many names to test uniqueness
        for _ in 0..(10 * expected_num_unique_names) {
            let name = make_name(&mut name_generator);
            name_set.insert(name);
        }

        assert!(name_set.len() >= expected_num_unique_names);
    }).is_ok()
}

pub fn test_ffi_integration_test() -> bool {
    use std::panic;

    panic::catch_unwind(|| {
        let mut petname = new_petname(2, "-");
        let name = make_name(&mut petname);
        assert!(name.contains("-"));
        assert!(!name.is_empty());
    }).is_ok()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashSet;

    fn split_name<'a>(name: &'a str, separator: &str) -> Vec<&'a str> {
        name.split(separator).collect()
    }

    #[test]
    fn generates_the_requested_num_words() {
        let mut gen1 = new_petname(1, "-");
        let mut gen2 = new_petname(2, "-");
        let mut gen3 = new_petname(3, "-");

        let one_word_name = make_name(&mut gen1);
        let tokens = split_name(&one_word_name, "-");
        assert_eq!(tokens.len(), 1);

        let two_word_name = make_name(&mut gen2);
        let tokens = split_name(&two_word_name, "-");
        assert_eq!(tokens.len(), 2);

        let three_word_name = make_name(&mut gen3);
        let tokens = split_name(&three_word_name, "-");
        assert_eq!(tokens.len(), 3);
    }

    #[test]
    fn uses_custom_separator() {
        let mut name_generator = new_petname(3, "_");
        let name = make_name(&mut name_generator);
        let tokens = split_name(&name, "_");
        assert_eq!(tokens.len(), 3);
        assert!(name.contains("_"));
    }

    #[test]
    fn generates_two_tokens_by_default() {
        let mut name_generator = new_petname(2, "-");
        let name = make_name(&mut name_generator);
        let tokens = split_name(&name, "-");
        assert_eq!(tokens.len(), 2);

        // Each token should be unique
        let unique_tokens: HashSet<_> = tokens.iter().collect();
        assert_eq!(unique_tokens.len(), tokens.len());
    }

    #[test]
    fn can_generate_at_least_hundred_unique_names() {
        let mut name_generator = new_petname(3, "-");
        let mut name_set = HashSet::new();
        let expected_num_unique_names = 100;

        // Generate many names to test uniqueness
        for _ in 0..(10 * expected_num_unique_names) {
            let name = make_name(&mut name_generator);
            name_set.insert(name);
        }

        assert!(name_set.len() >= expected_num_unique_names);
    }

    #[test]
    fn ffi_integration_test() {
        let mut petname = new_petname(2, "-");
        let name = make_name(&mut petname);
        assert!(name.contains("-"));
        assert!(!name.is_empty());
    }
}
