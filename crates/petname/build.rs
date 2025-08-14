fn main() {
    cxx_build::bridge("src/lib.rs")
        .flag_if_supported("-std=c++17")
        .compile("multipass-petname");

    println!("cargo:rerun-if-changed=src/lib.rs");
}
