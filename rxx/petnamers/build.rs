use std::fs;
fn main() {
    // Generate the .h and .cc files (_do not_ compile them)
    let out_dir = std::env::var("OUT_DIR").unwrap();
    let _ = cxx_build::bridge("src/lib.rs"); // drops the builder, just codegen
    fs::copy(format!("{}/lib.rs.h", out_dir), "./");
}
