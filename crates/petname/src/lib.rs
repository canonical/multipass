use rand::seq::SliceRandom;
use rand::thread_rng;
use std::fmt;

const ADJECTIVES: &str = include_str!("adjectives.txt");
const ADVERBS: &str = include_str!("adverbs.txt");
const NAMES: &str = include_str!("names.txt");

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum PetnameError {
    InvalidNumWords(i32),
    EmptyWordList(String),
}

impl fmt::Display for PetnameError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            PetnameError::InvalidNumWords(num) => {
                write!(f, "Invalid number of words: {}. Must be 1, 2, or 3", num)
            }
            PetnameError::EmptyWordList(list_name) => {
                write!(f, "Empty word list: {}", list_name)
            }
        }
    }
}

impl std::error::Error for PetnameError {}

#[derive(Debug)]
pub enum NumWords {
    One,
    Two,
    Three,
}

#[derive(Debug)]
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

    pub fn make_name(&self) -> Result<String, PetnameError> {
        let mut rng = thread_rng();
        let adjectives: Vec<&str> = ADJECTIVES.lines().collect();
        let adverbs: Vec<&str> = ADVERBS.lines().collect();
        let names: Vec<&str> = NAMES.lines().collect();

        let adjective = adjectives
            .choose(&mut rng)
            .ok_or_else(|| PetnameError::EmptyWordList("adjectives".to_string()))?;
        let adverb = adverbs
            .choose(&mut rng)
            .ok_or_else(|| PetnameError::EmptyWordList("adverbs".to_string()))?;
        let name = names
            .choose(&mut rng)
            .ok_or_else(|| PetnameError::EmptyWordList("names".to_string()))?;

        let result = match self.num_words {
            NumWords::One => name.to_string(),
            NumWords::Two => format!("{}{}{}", adjective, self.separator, name),
            NumWords::Three => format!(
                "{}{}{}{}{}",
                adverb, self.separator, adjective, self.separator, name
            ),
        };

        Ok(result)
    }
}

pub fn new_petname(num_words: i32, separator: &str) -> Result<Box<Petname>, PetnameError> {
    let num_words = match num_words {
        1 => NumWords::One,
        2 => NumWords::Two,
        3 => NumWords::Three,
        _ => return Err(PetnameError::InvalidNumWords(num_words)),
    };
    Ok(Box::new(Petname::new(num_words, separator)))
}

pub fn make_name(petname: &mut Petname) -> Result<String, PetnameError> {
    petname.make_name()
}

// CXX FFI Bridge for this module
#[cxx::bridge(namespace = "multipass::petname")]
mod ffi {
    extern "Rust" {
        type Petname;
        fn new_petname(num_words: i32, separator: &str) -> Result<Box<Petname>>;
        fn make_name(petname: &mut Petname) -> Result<String>;
    }
}
