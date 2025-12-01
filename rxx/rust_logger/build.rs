fn main() {
    // Generate the .h and .cc files (_do not_ compile them)
    let _ = cxx_build::bridge("src/lib.rs").out_dir("../../build/gen/multipass"); // drops the builder, just codegen
}
