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

use namegen::petname_error::PetnameError;
#[test]
fn test_error_display() {
    assert_eq!(
        PetnameError::InvalidWordNumber(5).to_string(),
        "Invalid word number: 5"
    );
    assert_eq!(
        PetnameError::InvalidSeparator(64).to_string(),
        "Invalid separator, ASCII code: 64"
    );
    assert_eq!(PetnameError::PoisonedMutex.to_string(), "Mutex is poisoned")
}
