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

#ifndef SIMPLE_PROCESS_SPEC_H
#define SIMPLE_PROCESS_SPEC_H

#include <memory>
#include <multipass/process/process_spec.h>

namespace multipass
{
std::unique_ptr<ProcessSpec> simple_process_spec(const QString& cmd, const QStringList& args = QStringList());
}
#endif // SIMPLE_PROCESS_SPEC_H
