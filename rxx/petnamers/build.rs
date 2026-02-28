fn main() {
    // Generate the .h and .cc files (_do not_ compile them)
    let _ = cxx_build::bridge("src/lib.rs"); // drops the builder, just codegen
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/name_generator_rs.cpp");
}
