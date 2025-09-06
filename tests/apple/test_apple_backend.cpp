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

#include "tests/common.h"
#include "tests/mock_logger.h"
#include "tests/mock_platform.h"
#include "tests/mock_process_factory.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_dir.h"
#include "tests/temp_file.h"

#include <src/platform/backends/hyperv/hyperv_virtual_machine_factory.h>

#include <multipass/format.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/virtual_machine_factory.h>

#include <QRegularExpression>

#include <stdexcept>
#include <tuple>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace multipass::test
{
}
