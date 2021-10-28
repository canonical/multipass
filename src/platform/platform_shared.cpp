/*
 * Copyright (C) 2020-2021 Canonical, Ltd.
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

#include "platform_shared.h"

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>

#include <QKeySequence>

namespace mp = multipass;
namespace mpp = multipass::platform;

// A few notes on this:
// 1) Some shortcuts may feel counter-intuitive. For example in a keyboard where pressing "shift+-" produces an
// underscore, "_" is still interpreted the same as "-". IOW, "shift+-" == "shift+_" != "_" (just like "u" is the same
// as "U").
// 2) QHotKey fails to register some of the shortcuts we accept here (e.g. "Media Play").
// 3) QKeySequence seems to have problems with AtlGr. QKeySequence("AltGr").toString() prints rubbish (that it does not
// interpret back to mean the same thing). Unfortunately it is not enough to specify "ú" when that's what the layout
// produces for AltGr+U. The Sequence "ú" is accepted and QHotKey registers it, but is gets triggered on "U" and not
// "AltGr+U".
// 4) There does not seem to be a way to specify numpad keys in QKeySequence (with or without numlock).
// 5) meta only seems to work with other modifiers (e.g. ctrl+meta+x works, but meta+x doesn't even though it is
// accepted with no warning)
QString mpp::interpret_hotkey(const QString& val)
{
    auto sequence = QKeySequence{val};
    auto ret = sequence.toString(QKeySequence::NativeText);

    if (ret.isEmpty() && !sequence.isEmpty())
        throw InvalidSettingException(mp::hotkey_key, val, "Invalid key sequence");
    if (sequence.count() > 1)
        throw InvalidSettingException(mp::hotkey_key, val, "Multiple key sequences are not supported");

    return ret;
}
