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

use named_lock::NamedLock;
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

fn get_target_scoped_lock_name() -> String {
    // Read target dir (or OUT_DIR as fallback)
    let target_dir = std::env::var("CARGO_TARGET_DIR")
        .unwrap_or_else(|_| std::env::var("OUT_DIR").unwrap_or_default());

    // Hash the path to create a clean, fixed-length lock identifier
    let mut hasher = DefaultHasher::new();
    target_dir.hash(&mut hasher);

    format!("cxx_build_{:x}", hasher.finish())
}

fn main() {
    let lock_name = get_target_scoped_lock_name();
    let lock = NamedLock::create(&lock_name).expect("Failed to create scoped named lock");
    let _guard = lock.lock().expect("Failed to acquire lock");
    // Generate the .h and .cc files (_do not_ compile them)
    let _ = cxx_build::bridge("src/lib.rs"); // drops the builder, just codegen
    println!("cargo:rerun-if-changed=src/lib.rs");
}
