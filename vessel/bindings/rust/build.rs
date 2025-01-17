extern crate bindgen;
extern crate build_deps;

use std::env;
use std::path::PathBuf;

use std::process::Command;

fn main() {
    build_deps::rerun_if_changed_paths("../../include/**").unwrap();
    build_deps::rerun_if_changed_paths("../../src/*/*.a").unwrap();
    build_deps::rerun_if_changed_paths("../../src/*/*.so").unwrap();

    // Tell cargo to tell rustc to link the library.
    //println!("cargo:rustc-link-lib=static=base");
    //println!("cargo:rustc-link-lib=static=net");
    //println!("cargo:rustc-link-lib=static=runtime");
    //println!("cargo:rustc-flags=-L /home/vessel/workspace/vessel/dist/lib64");


    // consult shared.mk for other libraries... sorry y'all.
    let output = Command::new("make")
        .args(&["-f", "../../Makefile", "print-RUNTIME_LIBS"])
        .output()
        .unwrap();
    for t in String::from_utf8_lossy(&output.stdout).split_whitespace() {
        if t.starts_with("-L") {
            println!("cargo:rustc-flags={}", t.replace("-L", "-L "));
        } else if t.contains("-l:lib") {
        } else if t.starts_with("-l") {
            println!("cargo:rustc-link-lib={}", t.replace("-l", ""));
        }
    }

    // The bindgen::Builder is the main entry point
    // to bindgen, and lets you build up options for
    // the resulting bindings.
    let bindings = bindgen::Builder::default()
        .clang_arg("-I../../include/vessel")
        .clang_arg("-I../../src/inc")
        .clang_arg("-DCONFIG_NR_CPUS=8192")
        .clang_arg("-DCONFIG_NODES_SHIFT=10")
        .clang_arg("-DVESSEL_UIPI")
        .clang_arg("-DDIRECTPATH")
        // The input header we would like to generate
        // bindings for.
        .header("shenango.h")
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
