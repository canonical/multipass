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
 * Authored by: Antoni Bertolin Monferrer <antoni.monferrer@canonical.com>
 *
 */

use std::fmt;

#[derive(Debug, PartialEq)]
pub enum PetnameError {
    InvalidWordNumber(i32),
    InvalidSeparator(i8),
    InternalStateError(i32),
    EmptyArrayError,
}

impl fmt::Display for PetnameError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            PetnameError::InvalidWordNumber(num) => write!(f, "Invalid word number: {}", num),
            PetnameError::InvalidSeparator(sep) => {
                write!(f, "Invalid separator, ASCII code: {}", sep)
            }
            PetnameError::EmptyArrayError => {
                write!(f, "Cannot choose an element of an empty array")
            }
            PetnameError::InternalStateError(num) => {
                write!(f, "Invalid internal state: numWords = {}", num)
            }
        }
    }
}
