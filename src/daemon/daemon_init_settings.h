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

#ifndef MULTIPASS_DAEMON_INIT_SETTINGS_H
#define MULTIPASS_DAEMON_INIT_SETTINGS_H

namespace multipass::daemon
{
void monitor_and_quit_on_settings_change(); // TODO replace with async restart in relevant settings handlers (see #2514)
void register_global_settings_handlers();
} // namespace multipass::daemon

#endif // MULTIPASS_DAEMON_INIT_SETTINGS_H
