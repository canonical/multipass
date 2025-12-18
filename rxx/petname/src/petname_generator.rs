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
 * Authored by: Antoni Bertolin <antoni.monferrer@canonical.com>
 *
 */

use macros::make_word_array;
use static_assertions as sa;

//Paths are relative to the workspace root
const ADJECTIVES: &[&str] = &make_word_array!("petname/src/adjectives.txt");
const ADVERBS: &[&str] = &make_word_array!("petname/src/adverbs.txt");
const NOUNS: &[&str] = &make_word_array!("petname/src/names.txt");

sa::const_assert!(!NOUNS.is_empty());
sa::const_assert!(!ADJECTIVES.is_empty());
sa::const_assert!(!ADVERBS.is_empty());
