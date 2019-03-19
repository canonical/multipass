/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_VERSION_COMPARITOR_H
#define MULTIPASS_VERSION_COMPARITOR_H

#include <QString>
// glibc defines major & minor macros which interfere with the class method names
#undef major
#undef minor

namespace multipass
{

/*
 * Version - parses Multipass' version string to permit comparison
 *
 * Multipass' internal version string is the output of "git describe". From its docs:
 *
 *   The command finds the most recent tag that is reachable from a commit. If the tag
 *   points to the commit, then only the tag is shown. Otherwise, it suffixes the tag
 *   name with the number of additional commits on top of the tagged object and the
 *   abbreviated object name of the most recent commit.
 *
 * That implies two forms will need to be accepted: "v0.5-full-148-g6565145" or "v0.5-full"
 * but note only the number in the tag name will actually be compared.
 */
class Version
{
public:
    Version(const QString &version_string);

    bool operator<(const Version &version);

    int major() const;
    int minor() const;
    QString modifier() const;

private:
    int ver_major;
    int ver_minor;
    QString ver_modifier;
};

} // namespace multipass
#endif // vVERSION_COMPARITOR_H
