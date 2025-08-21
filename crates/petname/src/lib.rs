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

// CXX FFI Bridge for this module
#[cxx::bridge(namespace = "multipass::petname")]
mod ffi {
    extern "Rust" {
        type Petname;
        fn new_petname(num_words: i32, separator: &str) -> Box<Petname>;
        fn make_name(petname: &mut Petname) -> String;
    }
}
