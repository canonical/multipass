/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

//! Shared build-script helpers for the `rxx` CXX crates.
//!
//! Every crate that exposes a CXX bridge needs to generate its `.h`/`.cc`
//! files during its build script while avoiding concurrent access to the
//! shared target directory. This crate centralises that logic so each
//! `build.rs` is a single call to [`generate_bridge`].

use named_lock::NamedLock;
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

/// Build a lock name scoped to the current target directory.
///
/// Hashing the path keeps the identifier clean and fixed-length while still
/// being unique per target directory.
fn target_scoped_lock_name() -> String {
    // Read target dir (or OUT_DIR as fallback)
    let target_dir = std::env::var("CARGO_TARGET_DIR")
        .unwrap_or_else(|_| std::env::var("OUT_DIR").unwrap_or_default());

    let mut hasher = DefaultHasher::new();
    target_dir.hash(&mut hasher);

    format!("cxx_build_{:x}", hasher.finish())
}

/// Generate the CXX bindings for `bridge_path` (e.g. `"src/lib.rs"`).
///
/// This only performs codegen of the `.h`/`.cc` files (it does *not* compile
/// them) while holding a target-scoped exclusive lock, and emits the
/// appropriate `cargo:rerun-if-changed` directive.
pub fn generate_bridge(bridge_path: &str) {
    let lock_name = target_scoped_lock_name();
    let lock = NamedLock::create(&lock_name).expect("Failed to create scoped named lock");
    // Hold exclusive write lock only while generating CXX bindings
    let _guard = lock.lock().expect("Failed to acquire lock");

    // Generate the .h and .cc files (_do not_ compile them)
    let _ = cxx_build::bridge(bridge_path); // drops the builder, just codegen
    println!("cargo:rerun-if-changed={bridge_path}");
}
